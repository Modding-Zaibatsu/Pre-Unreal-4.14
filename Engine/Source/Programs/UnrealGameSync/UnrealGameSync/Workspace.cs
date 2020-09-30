﻿// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	[Flags]
	enum WorkspaceUpdateOptions
	{
		Sync = 0x01,
		SyncSingleChange = 0x02,
		AutoResolveChanges = 0x04,
		GenerateProjectFiles = 0x08,
		SyncArchives = 0x10,
		Build = 0x20,
		UseIncrementalBuilds = 0x40,
		ScheduledBuild = 0x80,
		RunAfterSync = 0x100,
		OpenSolutionAfterSync = 0x200,
		SkipShaders = 0x400,
	}

	enum WorkspaceUpdateResult
	{
		Canceled,
		FailedToSync,
		FilesToResolve,
		FilesToClobber,
		FailedToCompile,
		FailedToCompileWithCleanWorkspace,
		Success,
	}

	class WorkspaceUpdateContext
	{
		public DateTime StartTime = DateTime.UtcNow;
		public int ChangeNumber;
		public WorkspaceUpdateOptions Options;
		public string[] SyncFilter;
		public Dictionary<string, string> ArchiveTypeToDepotPath = new Dictionary<string,string>();
		public Dictionary<string, bool> ClobberFiles = new Dictionary<string,bool>();
		public Dictionary<Guid,ConfigObject> DefaultBuildSteps;
		public List<ConfigObject> UserBuildStepObjects;
		public HashSet<Guid> CustomBuildSteps;
		public Dictionary<string, string> Variables;

		public WorkspaceUpdateContext(int InChangeNumber, WorkspaceUpdateOptions InOptions, string[] InSyncFilter, Dictionary<Guid, ConfigObject> InDefaultBuildSteps, List<ConfigObject> InUserBuildSteps, HashSet<Guid> InCustomBuildSteps, Dictionary<string, string> InVariables)
		{
			ChangeNumber = InChangeNumber;
			Options = InOptions;
			SyncFilter = InSyncFilter;
			DefaultBuildSteps = InDefaultBuildSteps;
			UserBuildStepObjects = InUserBuildSteps;
			CustomBuildSteps = InCustomBuildSteps;
			Variables = InVariables;
		}
	}

	class Workspace : IDisposable
	{
		readonly string[] DefaultBuildTargets =
		{
			"UnrealHeaderTool Win64 Development, 0.1",
			"$(EditorTarget) Win64 $(EditorConfiguration), 0.7",
			"ShaderCompileWorker Win64 Development, 0.8",
			"UnrealLightmass Win64 Development, 0.9",
			"CrashReportClient Win64 Shipping, 1.0",
		};

		const string BuildVersionFileName = "/Engine/Build/Build.version";
		const string VersionHeaderFileName = "/Engine/Source/Runtime/Launch/Resources/Version.h";
		const string ObjectVersionFileName = "/Engine/Source/Runtime/Core/Private/UObject/ObjectVersion.cpp";

		public readonly PerforceConnection Perforce;
		public readonly IReadOnlyList<string> SyncPaths;
		public readonly string LocalRootPath;
		public readonly string SelectedLocalFileName;
		public readonly string ClientRootPath;
		public readonly string SelectedClientFileName;
		public readonly string TelemetryProjectPath;
		Thread WorkerThread;
		TextWriter Log;
		bool bSyncing;
		ProgressValue Progress = new ProgressValue();

		public event Action<WorkspaceUpdateContext, WorkspaceUpdateResult, string> OnUpdateComplete;

		public Workspace(PerforceConnection InPerforce, string InLocalRootPath, string InSelectedLocalFileName, string InClientRootPath, string InSelectedClientFileName, int InInitialChangeNumber, int InLastBuiltChangeNumber, string InTelemetryProjectPath, TextWriter InLog)
		{
			Perforce = InPerforce;
			LocalRootPath = InLocalRootPath;
			SelectedLocalFileName = InSelectedLocalFileName;
			ClientRootPath = InClientRootPath;
			SelectedClientFileName = InSelectedClientFileName;
			CurrentChangeNumber = InInitialChangeNumber;
			PendingChangeNumber = InInitialChangeNumber;
			LastBuiltChangeNumber = InLastBuiltChangeNumber;
			TelemetryProjectPath = InTelemetryProjectPath;
			Log = InLog;

			List<string> SyncPaths = new List<string>();
			if(SelectedClientFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
			{
				SyncPaths.Add(ClientRootPath + "/*");
				SyncPaths.Add(ClientRootPath + "/Engine/...");
				SyncPaths.Add(PerforceUtils.GetClientOrDepotDirectoryName(SelectedClientFileName) + "/...");
			}
			else
			{
				SyncPaths.Add(ClientRootPath + "/...");
			}
			this.SyncPaths = SyncPaths.AsReadOnly();

			ProjectConfigFile = ReadProjectConfigFile(InLocalRootPath, InSelectedLocalFileName, Log);
		}

		public void Dispose()
		{
			CancelUpdate();
		}

		public ConfigFile ProjectConfigFile
		{
			get; private set;
		}

		public void Update(WorkspaceUpdateContext Context)
		{
			// Kill any existing sync
			CancelUpdate();

			// Set the initial progress message
			if(CurrentChangeNumber != Context.ChangeNumber)
			{
				PendingChangeNumber = Context.ChangeNumber;
				if(!Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
				{
					CurrentChangeNumber = -1;
					ProjectConfigFile = null;
				}
			}
			Progress.Clear();
			bSyncing = true;

			// Spawn the new thread
			WorkerThread = new Thread(x => UpdateWorkspace(Context));
			WorkerThread.Start();
		}

		public void CancelUpdate()
		{
			if(bSyncing)
			{
				Log.WriteLine("SYNC ABORTED");
				if(WorkerThread != null)
				{
					WorkerThread.Abort();
					WorkerThread.Join();
					WorkerThread = null;
				}
				PendingChangeNumber = CurrentChangeNumber;
				bSyncing = false;
			}
		}

		void UpdateWorkspace(WorkspaceUpdateContext Context)
		{
			string StatusMessage;

			WorkspaceUpdateResult Result = WorkspaceUpdateResult.FailedToSync;
			try
			{
				Result = UpdateWorkspaceInternal(Context, out StatusMessage);
				if(Result != WorkspaceUpdateResult.Success)
				{
					Log.WriteLine("{0}", StatusMessage);
				}
			}
			catch(ThreadAbortException)
			{
				StatusMessage = "Canceled.";
				Log.WriteLine("Canceled.");
			}
			catch(Exception Ex)
			{
				StatusMessage = "Failed with exception - " + Ex.ToString();
				Log.WriteException(Ex, "Failed with exception");
			}

			bSyncing = false;
			PendingChangeNumber = CurrentChangeNumber;

			if(OnUpdateComplete != null)
			{
				OnUpdateComplete(Context, Result, StatusMessage);
			}
		}

		WorkspaceUpdateResult UpdateWorkspaceInternal(WorkspaceUpdateContext Context, out string StatusMessage)
		{
			string CmdExe = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "cmd.exe");
			if(!File.Exists(CmdExe))
			{
				StatusMessage = String.Format("Missing {0}.", CmdExe);
				return WorkspaceUpdateResult.FailedToSync;
			}

			List<Tuple<string, TimeSpan>> Times = new List<Tuple<string,TimeSpan>>();

			int NumFilesSynced = 0;
			if(Context.Options.HasFlag(WorkspaceUpdateOptions.Sync) || Context.Options.HasFlag(WorkspaceUpdateOptions.SyncSingleChange))
			{
				using(TelemetryStopwatch Stopwatch = new TelemetryStopwatch("Sync", TelemetryProjectPath))
				{
					Log.WriteLine("Syncing to {0}...", PendingChangeNumber);

					// Find all the files that are out of date
					Progress.Set("Finding files to sync...");

					// Get the user's sync filter
					FileFilter UserFilter = null;
					if(Context.SyncFilter != null)
					{
						UserFilter = new FileFilter(FileFilterType.Include);
						UserFilter.AddRules(Context.SyncFilter.Select(x => x.Trim()).Where(x => x.Length > 0 && !x.StartsWith(";") && !x.StartsWith("#")));
					}

					// Find all the server changes, and anything that's opened for edit locally. We need to sync files we have open to schedule a resolve.
					List<string> SyncFiles = new List<string>();
					foreach(string SyncPath in SyncPaths)
					{
						List<PerforceFileRecord> SyncRecords;
						if(!Perforce.SyncPreview(SyncPath, PendingChangeNumber, !Context.Options.HasFlag(WorkspaceUpdateOptions.Sync), out SyncRecords, Log))
						{
							StatusMessage = String.Format("Couldn't enumerate changes matching {0}.", SyncPath);
							return WorkspaceUpdateResult.FailedToSync;
						}

						if(UserFilter != null)
						{
							SyncRecords.RemoveAll(x => !String.IsNullOrEmpty(x.ClientPath) && !MatchFilter(x.ClientPath, UserFilter));
						}

						SyncFiles.AddRange(SyncRecords.Select(x => x.DepotPath));

						List<PerforceFileRecord> OpenRecords;
						if(!Perforce.GetOpenFiles(SyncPath, out OpenRecords, Log))
						{
							StatusMessage = String.Format("Couldn't find open files matching {0}.", SyncPath);
							return WorkspaceUpdateResult.FailedToSync;
						}

						// don't force a sync on added files
						SyncFiles.AddRange(OpenRecords.Where(x => x.Action != "add" && x.Action != "branch" && x.Action != "move/add").Select(x => x.DepotPath));
					}

					// Filter out all the binaries that we don't want
					FileFilter Filter = new FileFilter(FileFilterType.Include);
					Filter.Exclude("..." + BuildVersionFileName);
					Filter.Exclude("..." + VersionHeaderFileName);
					Filter.Exclude("..." + ObjectVersionFileName);
					if(Context.Options.HasFlag(WorkspaceUpdateOptions.SkipShaders))
					{
						Filter.Exclude("*.usf");
					}
					SyncFiles.RemoveAll(x => !Filter.Matches(x));

					// Sync them all
					List<string> TamperedFiles = new List<string>();
					HashSet<string> RemainingFiles = new HashSet<string>(SyncFiles, StringComparer.InvariantCultureIgnoreCase);
					if(!Perforce.Sync(SyncFiles, PendingChangeNumber, Record => UpdateSyncProgress(Record, RemainingFiles, SyncFiles.Count), TamperedFiles, Log))
					{
						StatusMessage = "Aborted sync due to errors.";
						return WorkspaceUpdateResult.FailedToSync;
					}

					// If any files need to be clobbered, defer to the main thread to figure out which ones
					if(TamperedFiles.Count > 0)
					{
						int NumNewFilesToClobber = 0;
						foreach(string TamperedFile in TamperedFiles)
						{
							if(!Context.ClobberFiles.ContainsKey(TamperedFile))
							{
								Context.ClobberFiles[TamperedFile] = true;
								NumNewFilesToClobber++;
							}
						}
						if(NumNewFilesToClobber > 0)
						{
							StatusMessage = String.Format("Cancelled sync after checking files to clobber ({0} new files).", NumNewFilesToClobber);
							return WorkspaceUpdateResult.FilesToClobber;
						}
						foreach(string TamperedFile in TamperedFiles)
						{
							if(Context.ClobberFiles[TamperedFile] && !Perforce.ForceSync(TamperedFile, PendingChangeNumber, Log))
							{
								StatusMessage = String.Format("Couldn't sync {0}.", TamperedFile);
								return WorkspaceUpdateResult.FailedToSync;
							}
						}
					}

					ConfigFile NewProjectConfigFile = null;
					if(Context.Options.HasFlag(WorkspaceUpdateOptions.Sync))
					{
						// Read the new config file
						NewProjectConfigFile = ReadProjectConfigFile(LocalRootPath, SelectedLocalFileName, Log);

						// Get the branch name
						string BranchOrStreamName;
						if(!Perforce.GetActiveStream(out BranchOrStreamName, Log))
						{
							string DepotFileName;
							if(!Perforce.ConvertToDepotPath(ClientRootPath + "/GenerateProjectFiles.bat", out DepotFileName, Log))
							{
								StatusMessage = String.Format("Couldn't determine branch name for {0}.", SelectedClientFileName);
								return WorkspaceUpdateResult.FailedToSync;
							}
							BranchOrStreamName = PerforceUtils.GetClientOrDepotDirectoryName(DepotFileName);
						}

						// Find the last code change before this changelist. For consistency in versioning between local builds and precompiled binaries, we need to use the last submitted code changelist as our version number.
						List<PerforceChangeSummary> CodeChanges;
						if(!Perforce.FindChanges(new string[]{ ".cs", ".h", ".cpp", ".usf" }.SelectMany(x => SyncPaths.Select(y => String.Format("{0}{1}@<={2}", y, x, PendingChangeNumber))), 1, out CodeChanges, Log))
						{
							StatusMessage = String.Format("Couldn't determine last code changelist before CL {0}.", PendingChangeNumber);
							return WorkspaceUpdateResult.FailedToSync;
						}
						if(CodeChanges.Count == 0)
						{
							StatusMessage = String.Format("Could not find any code changes before CL {0}.", PendingChangeNumber);
							return WorkspaceUpdateResult.FailedToSync;
						}

						// Get the last code change
						int VersionChangeNumber;
						if(NewProjectConfigFile.GetValue("Options.VersionToLastCodeChange", true))
						{
							VersionChangeNumber = CodeChanges.Max(x => x.Number);
						}
						else
						{
							VersionChangeNumber = PendingChangeNumber;
						}

						// Update the version files
						if(NewProjectConfigFile.GetValue("Options.UseFastModularVersioning", false))
						{
							Dictionary<string, string> BuildVersionStrings = new Dictionary<string,string>();
							BuildVersionStrings["\"Changelist\":"] = String.Format(" {0},", VersionChangeNumber);
							BuildVersionStrings["\"BranchName\":"] = String.Format(" \"{0}\"", BranchOrStreamName.Replace('/', '+'));
							if(!UpdateVersionFile(ClientRootPath + BuildVersionFileName, BuildVersionStrings, PendingChangeNumber))
							{
								StatusMessage = String.Format("Failed to update {0}.", BuildVersionFileName);
								return WorkspaceUpdateResult.FailedToSync;
							}

							Dictionary<string, string> VersionHeaderStrings = new Dictionary<string,string>();
							VersionHeaderStrings["#define ENGINE_IS_PROMOTED_BUILD"] = " (0)";
							VersionHeaderStrings["#define BUILT_FROM_CHANGELIST"] = " 0";
							VersionHeaderStrings["#define BRANCH_NAME"] = " \"" + BranchOrStreamName.Replace('/', '+') + "\"";
							if(!UpdateVersionFile(ClientRootPath + VersionHeaderFileName, VersionHeaderStrings, PendingChangeNumber))
							{
								StatusMessage = String.Format("Failed to update {0}.", VersionHeaderFileName);
								return WorkspaceUpdateResult.FailedToSync;
							}
							if(!UpdateVersionFile(ClientRootPath + ObjectVersionFileName, new Dictionary<string,string>(), PendingChangeNumber))
							{
								StatusMessage = String.Format("Failed to update {0}.", ObjectVersionFileName);
								return WorkspaceUpdateResult.FailedToSync;
							}
						}
						else
						{
							if(!UpdateVersionFile(ClientRootPath + BuildVersionFileName, new Dictionary<string, string>(), PendingChangeNumber))
							{
								StatusMessage = String.Format("Failed to update {0}", BuildVersionFileName);
								return WorkspaceUpdateResult.FailedToSync;
							}

							Dictionary<string, string> VersionStrings = new Dictionary<string,string>();
							VersionStrings["#define ENGINE_VERSION"] = " " + VersionChangeNumber.ToString();
							VersionStrings["#define ENGINE_IS_PROMOTED_BUILD"] = " (0)";
							VersionStrings["#define BUILT_FROM_CHANGELIST"] = " " + VersionChangeNumber.ToString();
							VersionStrings["#define BRANCH_NAME"] = " \"" + BranchOrStreamName.Replace('/', '+') + "\"";
							if(!UpdateVersionFile(ClientRootPath + VersionHeaderFileName, VersionStrings, PendingChangeNumber))
							{
								StatusMessage = String.Format("Failed to update {0}", VersionHeaderFileName);
								return WorkspaceUpdateResult.FailedToSync;
							}
							if(!UpdateVersionFile(ClientRootPath + ObjectVersionFileName, VersionStrings, PendingChangeNumber))
							{
								StatusMessage = String.Format("Failed to update {0}", ObjectVersionFileName);
								return WorkspaceUpdateResult.FailedToSync;
							}
						}

						// Remove all the receipts for build targets in this branch
						if(SelectedClientFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
						{
							Perforce.Sync(PerforceUtils.GetClientOrDepotDirectoryName(SelectedClientFileName) + "/Build/Receipts/...#0", Log);
						}
					}

					// Check if there are any files which need resolving
					List<PerforceFileRecord> UnresolvedFiles;
					if(!FindUnresolvedFiles(SyncPaths, out UnresolvedFiles))
					{
						StatusMessage = "Couldn't get list of unresolved files.";
						return WorkspaceUpdateResult.FailedToSync;
					}
					if(UnresolvedFiles.Count > 0 && Context.Options.HasFlag(WorkspaceUpdateOptions.AutoResolveChanges))
					{
						foreach (PerforceFileRecord UnresolvedFile in UnresolvedFiles)
						{
							Perforce.AutoResolveFile(UnresolvedFile.DepotPath, Log);
						}
						if(!FindUnresolvedFiles(SyncPaths, out UnresolvedFiles))
						{
							StatusMessage = "Couldn't get list of unresolved files.";
							return WorkspaceUpdateResult.FailedToSync;
						}
					}
					if(UnresolvedFiles.Count > 0)
					{
						Log.WriteLine("{0} files need resolving:", UnresolvedFiles.Count);
						foreach(PerforceFileRecord UnresolvedFile in UnresolvedFiles)
						{
							Log.WriteLine("  {0}", UnresolvedFile.ClientPath);
						}
						StatusMessage = "Files need resolving.";
						return WorkspaceUpdateResult.FilesToResolve;
					}

					// Update the current change number. Everything else happens for the new change.
					if(Context.Options.HasFlag(WorkspaceUpdateOptions.Sync))
					{
						ProjectConfigFile = NewProjectConfigFile;
						CurrentChangeNumber = PendingChangeNumber;
					}

					// Update the timing info
					Times.Add(new Tuple<string,TimeSpan>("Sync", Stopwatch.Stop("Success")));

					// Save the number of files synced
					NumFilesSynced = SyncFiles.Count;
					Log.WriteLine();
				}
			}

			// Extract an archive from the depot path
			if(Context.Options.HasFlag(WorkspaceUpdateOptions.SyncArchives))
			{
				using(TelemetryStopwatch Stopwatch = new TelemetryStopwatch("Archives", TelemetryProjectPath))
				{
					// Create the directory for extracted archive manifests
					string ManifestDirectoryName;
					if(SelectedLocalFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
					{
						ManifestDirectoryName = Path.Combine(Path.GetDirectoryName(SelectedLocalFileName), "Saved", "UnrealGameSync");
					}
					else
					{
						ManifestDirectoryName = Path.Combine(Path.GetDirectoryName(SelectedLocalFileName), "Engine", "Saved", "UnrealGameSync");
					}
					Directory.CreateDirectory(ManifestDirectoryName);

					// Sync and extract (or just remove) the given archives
					foreach(KeyValuePair<string, string> ArchiveTypeAndDepotPath in Context.ArchiveTypeToDepotPath)
					{
						// Remove any existing binaries
						string ManifestFileName = Path.Combine(ManifestDirectoryName, String.Format("{0}.zipmanifest", ArchiveTypeAndDepotPath.Key));
						if(File.Exists(ManifestFileName))
						{
							Log.WriteLine("Removing {0} binaries...", ArchiveTypeAndDepotPath.Key);
							Progress.Set(String.Format("Removing {0} binaries...", ArchiveTypeAndDepotPath.Key), 0.0f);
							ArchiveUtils.RemoveExtractedFiles(LocalRootPath, ManifestFileName, Progress, Log);
							Log.WriteLine();
						}

						// If we have a new depot path, sync it down and extract it
						if(ArchiveTypeAndDepotPath.Value != null)
						{
							string TempZipFileName = Path.GetTempFileName();
							try
							{
								Log.WriteLine("Syncing {0} binaries...", ArchiveTypeAndDepotPath.Key.ToLowerInvariant());
								Progress.Set(String.Format("Syncing {0} binaries...", ArchiveTypeAndDepotPath.Key.ToLowerInvariant()), 0.0f);
								if(!Perforce.PrintToFile(ArchiveTypeAndDepotPath.Value, TempZipFileName, Log) || new FileInfo(TempZipFileName).Length == 0)
								{
									StatusMessage = String.Format("Couldn't read {0}", ArchiveTypeAndDepotPath.Value);
									return WorkspaceUpdateResult.FailedToSync;
								}
								ArchiveUtils.ExtractFiles(TempZipFileName, LocalRootPath, ManifestFileName, Progress, Log);
								Log.WriteLine();
							}
							finally
							{
								File.SetAttributes(TempZipFileName, FileAttributes.Normal);
								File.Delete(TempZipFileName);
							}
						}
					}

					// Add the finish time
					Times.Add(new Tuple<string,TimeSpan>("Archive", Stopwatch.Stop("Success")));
				}
			}

			// Generate project files in the workspace
			if(Context.Options.HasFlag(WorkspaceUpdateOptions.GenerateProjectFiles))
			{
				using(TelemetryStopwatch Stopwatch = new TelemetryStopwatch("Prj gen", TelemetryProjectPath))
				{
					Progress.Set("Generating project files...", 0.0f);

					string ProjectFileArgument = "";
					if(SelectedLocalFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
					{
						ProjectFileArgument = String.Format("\"{0}\" ", SelectedLocalFileName);
					}

					string CommandLine = String.Format("/C \"\"{0}\" {1}-progress\"", Path.Combine(LocalRootPath, "GenerateProjectFiles.bat"), ProjectFileArgument);

					Log.WriteLine("Generating project files...");
					Log.WriteLine("gpf> Running {0} {1}", CmdExe, CommandLine);

					int GenerateProjectFilesResult = Utility.ExecuteProcess(CmdExe, CommandLine, null, new ProgressTextWriter(Progress, new PrefixedTextWriter("gpf> ", Log)));
					if(GenerateProjectFilesResult != 0)
					{
						StatusMessage = String.Format("Failed to generate project files (exit code {0}).", GenerateProjectFilesResult);
						return WorkspaceUpdateResult.FailedToCompile;
					}

					Log.WriteLine();
					Times.Add(new Tuple<string,TimeSpan>("Prj gen", Stopwatch.Stop("Success")));
				}
			}

			// Build everything using MegaXGE
			if(Context.Options.HasFlag(WorkspaceUpdateOptions.Build))
			{
				// Compile all the build steps together
				Dictionary<Guid, ConfigObject> BuildStepObjects = Context.DefaultBuildSteps.ToDictionary(x => x.Key, x => new ConfigObject(x.Value));
				BuildStep.MergeBuildStepObjects(BuildStepObjects, ProjectConfigFile.GetValues("Build.Step", new string[0]).Select(x => new ConfigObject(x)));
				BuildStep.MergeBuildStepObjects(BuildStepObjects, Context.UserBuildStepObjects);

				// Construct build steps from them
				List<BuildStep> BuildSteps = BuildStepObjects.Values.Select(x => new BuildStep(x)).OrderBy(x => x.OrderIndex).ToList();
				if(Context.CustomBuildSteps != null && Context.CustomBuildSteps.Count > 0)
				{
					BuildSteps.RemoveAll(x => !Context.CustomBuildSteps.Contains(x.UniqueId));
				}
				else if(Context.Options.HasFlag(WorkspaceUpdateOptions.ScheduledBuild))
				{
					BuildSteps.RemoveAll(x => !x.bScheduledSync);
				}
				else
				{
					BuildSteps.RemoveAll(x => !x.bNormalSync);
				}

				// Check if the last successful build was before a change that we need to force a clean for
				bool bForceClean = false;
				if(LastBuiltChangeNumber != 0)
				{
					foreach(string CleanBuildChange in ProjectConfigFile.GetValues("ForceClean.Changelist", new string[0]))
					{
						int ChangeNumber;
						if(int.TryParse(CleanBuildChange, out ChangeNumber))
						{
							if((LastBuiltChangeNumber >= ChangeNumber) != (CurrentChangeNumber >= ChangeNumber))
							{
								Log.WriteLine("Forcing clean build due to changelist {0}.", ChangeNumber);
								Log.WriteLine();
								bForceClean = true;
								break;
							}
						}
					}
				}

				// Execute them all
				string TelemetryEventName = (Context.UserBuildStepObjects.Count > 0)? "CustomBuild" : Context.Options.HasFlag(WorkspaceUpdateOptions.UseIncrementalBuilds) ? "Compile" : "FullCompile";
				using(TelemetryStopwatch Stopwatch = new TelemetryStopwatch(TelemetryEventName, TelemetryProjectPath))
				{
					Progress.Set("Starting build...", 0.0f);

					// Check we've built UBT (it should have been compiled by generating project files)
					string UnrealBuildToolPath = Path.Combine(LocalRootPath, "Engine", "Binaries", "DotNET", "UnrealBuildTool.exe");
					if(!File.Exists(UnrealBuildToolPath))
					{
						StatusMessage = String.Format("Couldn't find {0}", UnrealBuildToolPath);
						return WorkspaceUpdateResult.FailedToCompile;
					}

					// Execute all the steps
					float MaxProgressFraction = 0.0f;
					foreach (BuildStep Step in BuildSteps)
					{
						MaxProgressFraction += (float)Step.EstimatedDuration / (float)Math.Max(BuildSteps.Sum(x => x.EstimatedDuration), 1);

						Progress.Set(Step.StatusText);
						Progress.Push(MaxProgressFraction);

						Log.WriteLine(Step.StatusText);

						switch(Step.Type)
						{
							case BuildStepType.Compile:
								using(TelemetryStopwatch StepStopwatch = new TelemetryStopwatch("Compile:" + Step.Target, TelemetryProjectPath))
								{
									string CommandLine = String.Format("{0} {1} {2} {3} -NoHotReloadFromIDE", Step.Target, Step.Platform, Step.Configuration, Utility.ExpandVariables(Step.Arguments, Context.Variables));
									if(!Context.Options.HasFlag(WorkspaceUpdateOptions.UseIncrementalBuilds) || bForceClean)
									{
										Log.WriteLine("ubt> Running {0} {1} -clean", UnrealBuildToolPath, CommandLine);
										Utility.ExecuteProcess(UnrealBuildToolPath, CommandLine + " -clean", null, new ProgressTextWriter(Progress, new PrefixedTextWriter("ubt> ", Log)));
									}

									Log.WriteLine("ubt> Running {0} {1} -progress", UnrealBuildToolPath, CommandLine);

									int ResultFromBuild = Utility.ExecuteProcess(UnrealBuildToolPath, CommandLine + " -progress", null, new ProgressTextWriter(Progress, new PrefixedTextWriter("ubt> ", Log)));
									if(ResultFromBuild != 0)
									{
										StatusMessage = String.Format("Failed to compile {0}.", Step.Target);
										return (HasModifiedSourceFiles() || Context.UserBuildStepObjects.Count > 0)? WorkspaceUpdateResult.FailedToCompile : WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace;
									}
								}
								break;
							case BuildStepType.Cook:
								using(TelemetryStopwatch StepStopwatch = new TelemetryStopwatch("Cook/Launch: " + Path.GetFileNameWithoutExtension(Step.FileName), TelemetryProjectPath))
								{
									string LocalRunUAT = Path.Combine(LocalRootPath, "Engine", "Build", "BatchFiles", "RunUAT.bat");
									string Arguments = String.Format("/C \"\"{0}\" -profile=\"{1}\"\"", LocalRunUAT, Path.Combine(LocalRootPath, Step.FileName));
									Log.WriteLine("uat> Running {0} {1}", LocalRunUAT, Arguments);

									int ResultFromUAT = Utility.ExecuteProcess(CmdExe, Arguments, null, new ProgressTextWriter(Progress, new PrefixedTextWriter("uat> ", Log)));
									if(ResultFromUAT != 0)
									{
										StatusMessage = String.Format("Cook failed. ({0})", ResultFromUAT);
										return WorkspaceUpdateResult.FailedToCompile;
									}
								}
								break;
							case BuildStepType.Other:
								using(TelemetryStopwatch StepStopwatch = new TelemetryStopwatch("Custom: " + Path.GetFileNameWithoutExtension(Step.FileName), TelemetryProjectPath))
								{
									string ToolFileName = Path.Combine(LocalRootPath, Utility.ExpandVariables(Step.FileName, Context.Variables));
									string ToolArguments = Utility.ExpandVariables(Step.Arguments, Context.Variables);
									Log.WriteLine("tool> Running {0} {1}", ToolFileName, ToolArguments);

									if(Step.bUseLogWindow)
									{
										int ResultFromTool = Utility.ExecuteProcess(ToolFileName, ToolArguments, null, new ProgressTextWriter(Progress, new PrefixedTextWriter("tool> ", Log)));
										if(ResultFromTool != 0)
										{
											StatusMessage = String.Format("Tool terminated with exit code {0}.", ResultFromTool);
											return WorkspaceUpdateResult.FailedToCompile;
										}
									}
									else
									{
										using(Process.Start(ToolFileName, ToolArguments))
										{
										}
									}
								}
								break;
						}

						Log.WriteLine();
						Progress.Pop();
					}

					Times.Add(new Tuple<string,TimeSpan>("Build", Stopwatch.Stop("Success")));
				}

				// Update the last successful build change number
				if(Context.CustomBuildSteps == null || Context.CustomBuildSteps.Count == 0)
				{
					LastBuiltChangeNumber = CurrentChangeNumber;
				}
			}

			// Write out all the timing information
			Log.WriteLine("Total time : " + FormatTime(Times.Sum(x => (long)(x.Item2.TotalMilliseconds / 1000))));
			foreach(Tuple<string, TimeSpan> Time in Times)
			{
				Log.WriteLine("   {0,-8}: {1}", Time.Item1, FormatTime((long)(Time.Item2.TotalMilliseconds / 1000)));
			}
			if(NumFilesSynced > 0)
			{
				Log.WriteLine("{0} files synced.", NumFilesSynced);
			}

			Log.WriteLine();
			Log.WriteLine("UPDATE SUCCEEDED");

			StatusMessage = "Update succeeded";
			return WorkspaceUpdateResult.Success;
		}

		bool MatchFilter(string FileName, FileFilter Filter)
		{
			bool bMatch = true;
			if(FileName.StartsWith(LocalRootPath, StringComparison.InvariantCultureIgnoreCase))
			{
				if(!Filter.Matches(FileName.Substring(LocalRootPath.Length)))
				{
					bMatch = false;
				}
			}
			return bMatch;
		}

		static ConfigFile ReadProjectConfigFile(string LocalRootPath, string SelectedLocalFileName, TextWriter Log)
		{
			// Find the valid config file paths
			List<string> ProjectConfigFileNames = new List<string>();
			ProjectConfigFileNames.Add(Path.Combine(LocalRootPath, "Engine", "Programs", "UnrealGameSync", "UnrealGameSync.ini"));
			if(SelectedLocalFileName.EndsWith(".uproject", StringComparison.InvariantCultureIgnoreCase))
			{
				ProjectConfigFileNames.Add(Path.Combine(Path.GetDirectoryName(SelectedLocalFileName), "Build", "UnrealGameSync.ini"));
			}

			// Read them in
			ConfigFile ProjectConfig = new ConfigFile();
			foreach(string ProjectConfigFileName in ProjectConfigFileNames)
			{
				if(File.Exists(ProjectConfigFileName))
				{
					try
					{
						string[] Lines = File.ReadAllLines(ProjectConfigFileName);
						ProjectConfig.Parse(Lines);
						Log.WriteLine("Read config file from {0}", ProjectConfigFileName);
					}
					catch(Exception Ex)
					{
						Log.WriteLine("Failed to read config file from {0}: {1}", ProjectConfigFileName, Ex.ToString());
					}
				}
			}
			return ProjectConfig;
		}

		static string FormatTime(long Seconds)
		{
			if(Seconds >= 60)
			{
				return String.Format("{0,3}m {1:00}s", Seconds / 60, Seconds % 60);
			}
			else
			{
				return String.Format("     {0,2}s", Seconds);
			}
		}

		bool HasModifiedSourceFiles()
		{
			List<PerforceFileRecord> OpenFiles;
			if(!Perforce.GetOpenFiles(ClientRootPath + "/...", out OpenFiles, Log))
			{
				return true;
			}
			if(OpenFiles.Any(x => x.DepotPath.IndexOf("/Source/", StringComparison.InvariantCultureIgnoreCase) != -1))
			{
				return true;
			}
			return false;
		}

		bool FindUnresolvedFiles(IEnumerable<string> SyncPaths, out List<PerforceFileRecord> UnresolvedFiles)
		{
			UnresolvedFiles = new List<PerforceFileRecord>();
			foreach(string SyncPath in SyncPaths)
			{
				List<PerforceFileRecord> Records;
				if(!Perforce.GetUnresolvedFiles(SyncPath, out Records, Log))
				{
					Log.WriteLine("Couldn't find open files matching {0}", SyncPath);
					return false;
				}
				UnresolvedFiles.AddRange(Records);
			}
			return true;
		}

		void UpdateSyncProgress(PerforceFileRecord Record, HashSet<string> RemainingFiles, int NumFiles)
		{
			RemainingFiles.Remove(Record.DepotPath);

			string Message = String.Format("Syncing files... ({0}/{1})", NumFiles - RemainingFiles.Count, NumFiles);
			float Fraction = Math.Min((float)(NumFiles - RemainingFiles.Count) / (float)NumFiles, 1.0f);
			Progress.Set(Message, Fraction);

			Log.WriteLine("p4>   {0} {1}", Record.Action, Record.ClientPath);
		}

		bool UpdateVersionFile(string LocalPath, Dictionary<string, string> VersionStrings, int ChangeNumber)
		{
			PerforceWhereRecord WhereRecord;
			if(!Perforce.Where(LocalPath, out WhereRecord, Log))
			{
				Log.WriteLine("P4 where failed for {0}", LocalPath);
				return false;
			}

			List<string> Lines;
			if(!Perforce.Print(String.Format("{0}@{1}", WhereRecord.DepotPath, ChangeNumber), out Lines, Log))
			{
				Log.WriteLine("Couldn't get default contents of {0}", WhereRecord.DepotPath);
				return false;
			}

			StringWriter Writer = new StringWriter();
			foreach(string Line in Lines)
			{
				string NewLine = Line;
				foreach(KeyValuePair<string, string> VersionString in VersionStrings)
				{
					if(UpdateVersionLine(ref NewLine, VersionString.Key, VersionString.Value))
					{
						break;
					}
				}
				Writer.WriteLine(NewLine);
			}

			return WriteVersionFile(WhereRecord, Writer.ToString());
		}

		bool WriteVersionFile(PerforceWhereRecord WhereRecord, string NewText)
		{
			try
			{
				if(File.Exists(WhereRecord.LocalPath) && File.ReadAllText(WhereRecord.LocalPath) == NewText)
				{
					Log.WriteLine("Ignored {0}; contents haven't changed", WhereRecord.LocalPath);
				}
				else
				{
					Utility.ForceDeleteFile(WhereRecord.LocalPath);
					if(WhereRecord.DepotPath != null)
					{
						Perforce.Sync(WhereRecord.DepotPath + "#0", Log);
					}
					File.WriteAllText(WhereRecord.LocalPath, NewText);
					Log.WriteLine("Written {0}", WhereRecord.LocalPath);
				}
				return true;
			}
			catch(Exception Ex)
			{
				Log.WriteException(Ex, "Failed to write to {0}.", WhereRecord.LocalPath);
				return false;
			}
		}

		bool UpdateVersionLine(ref string Line, string Prefix, string Suffix)
		{
			int LineIdx = 0;
			int PrefixIdx = 0;
			for(;;)
			{
				string PrefixToken = ReadToken(Prefix, ref PrefixIdx);
				if(PrefixToken == null)
				{
					break;
				}

				string LineToken = ReadToken(Line, ref LineIdx);
				if(LineToken == null || LineToken != PrefixToken)
				{
					return false;
				}
			}
			Line = Line.Substring(0, LineIdx) + Suffix;
			return true;
		}

		string ReadToken(string Line, ref int LineIdx)
		{
			for(;; LineIdx++)
			{
				if(LineIdx == Line.Length)
				{
					return null;
				}
				else if(!Char.IsWhiteSpace(Line[LineIdx]))
				{
					break;
				}
			}

			int StartIdx = LineIdx++;
			if(Char.IsLetterOrDigit(Line[StartIdx]) || Line[StartIdx] == '_')
			{
				while(LineIdx < Line.Length && (Char.IsLetterOrDigit(Line[LineIdx]) || Line[LineIdx] == '_'))
				{
					LineIdx++;
				}
			}

			return Line.Substring(StartIdx, LineIdx - StartIdx);
		}

		public bool IsBusy()
		{
			return bSyncing;
		}

		public Tuple<string, float> CurrentProgress
		{
			get { return Progress.Current; }
		}

		public int CurrentChangeNumber
		{
			get;
			private set;
		}

		public int PendingChangeNumber
		{
			get;
			private set;
		}

		public int LastBuiltChangeNumber
		{
			get;
			private set;
		}

		public string ClientName
		{
			get { return Perforce.ClientName; }
		}
	}
}
