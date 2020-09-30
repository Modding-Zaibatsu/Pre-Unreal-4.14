﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Tools.CrashReporter.CrashReportCommon;

namespace Tools.CrashReporter.CrashReportProcess
{
	class StatusReporting : IDisposable
	{
		public void InitCounters(IEnumerable<string> CounterNames)
		{
			foreach (string CounterName in CounterNames)
			{
				if (!Counters.ContainsKey(CounterName))
				{
					Counters.Add(CounterName, 0);
				}
			}
		}

		public void InitFolderMonitors(IDictionary<string, string> InFolderMonitors)
		{
			// Collapse list of folders and labels into a set of drive letters and one or more labels
			foreach (var FolderAndLabel in InFolderMonitors)
			{
				if (!string.IsNullOrWhiteSpace(FolderAndLabel.Key))
				{
					string Drive;
					if (CrashReportCommon.StorageSpaceHelper.TryGetDriveLetter(FolderAndLabel.Key, out Drive))
					{
						if (FolderMonitors.ContainsKey(Drive))
						{
							// Add label to existing value
							FolderMonitors[Drive] = FolderMonitors[Drive] + ", " + FolderAndLabel.Value;
						}
						else
						{
							FolderMonitors.Add(Drive, FolderAndLabel.Value);
						}
					}
				}
			}
		}

		public void InitQueue(string QueueName, string QueueLocation)
		{
			QueueLocations.Add(QueueName, QueueLocation);
			QueueSizes.Add(QueueName, 0);
		}

		public void SetQueueSize(string QueueName, int Size)
		{
			lock (DataLock)
			{
				QueueSizes[QueueName] = Size;
                SetQueueSizesCallCount++;
			}
		}

		public void IncrementCount(string CountName)
		{
			lock (DataLock)
			{
				if (Counters.ContainsKey(CountName))
				{
					Counters[CountName]++;
				}
				else
				{
					Counters.Add(CountName, 1);
				}
			}
		}

		public void Alert(string AlertKey, string AlertText, int RepeatMinimumMinutes)
		{
			// Do "no repeat" check here of recent alerts and block duplicates based on timer (use AlertKey)
			if (CheckRecentAlerts(AlertKey, RepeatMinimumMinutes))
			{
				// Report an alert condition to Slack immediately
				CrashReporterProcessServicer.WriteSlack("@channel ALERT: " + AlertText);
			}
		}

		public void AlertOnLowDisk(string Filepath, float AlertThresholdPercent)
		{
			try
			{
				string Drive;
				if (!StorageSpaceHelper.TryGetDriveLetter(Filepath, out Drive))
				{
					throw new CrashReporterException("Failed to get drive letter for path " + Filepath);
				}

				Int64 FreeSpace;
				float FreePercent;
				if (StorageSpaceHelper.TryGetSpaceAvailable(Drive, out FreeSpace, out FreePercent))
				{
					if (FreePercent < AlertThresholdPercent)
					{
						Alert("AlertOnLowDisk" + Drive, "Low disk space warning on " + Drive + " =>> " + GetDiskSpaceString(FreeSpace, FreePercent), 3 * Config.Default.SlackAlertRepeatMinimumMinutes);
					}
				}
				else
				{
					CrashReporterProcessServicer.WriteEvent("Failed to read disk space for " + Drive);
				}
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException("AlertOnLowDisk failed: " + Ex, Ex);
			}
        }

		public void Start()
		{
			CrashReporterProcessServicer.WriteSlack(string.Format("CRP started (version {0})", Config.Default.VersionString));

			StringBuilder StartupMessage = new StringBuilder();

			StartupMessage.AppendLine("Queues...");
			foreach (var Queue in QueueLocations)
			{
				StartupMessage.AppendLine("> " + Queue.Key + " @ " + Queue.Value);
			}
			CrashReporterProcessServicer.WriteSlack(StartupMessage.ToString());

			ReporterTasks = new[]
			{
				new StatusReportLoop(TimeSpan.FromMinutes(Config.Default.MinutesBetweenQueueSizeReports), InPeriod =>
				{
					StringBuilder StatusReportMessage = new StringBuilder();
					lock (DataLock)
					{
						Dictionary<string, int> CountsInPeriod = GetCountsInPeriod(Counters, CountersAtLastReport);

						if (SetQueueSizesCallCount >= QueueSizes.Count)
						{
							int ProcessingStartedInPeriodReceiver = 0;
							CountsInPeriod.TryGetValue(StatusReportingEventNames.ProcessingStartedReceiverEvent, out ProcessingStartedInPeriodReceiver);
							int ProcessingStartedInPeriodDataRouter = 0;
							CountsInPeriod.TryGetValue(StatusReportingEventNames.ProcessingStartedDataRouterEvent, out ProcessingStartedInPeriodDataRouter);
							int ProcessingStartedInPeriod = ProcessingStartedInPeriodReceiver + ProcessingStartedInPeriodDataRouter;

							if (ProcessingStartedInPeriod > 0)
							{
								int QueueSizeSum = QueueSizes.Values.Sum();
								TimeSpan MeanWaitTime =
									TimeSpan.FromTicks((long)(0.5*InPeriod.Ticks*(QueueSizeSum + QueueSizeSumAtLastReport)/ProcessingStartedInPeriod));
								QueueSizeSumAtLastReport = QueueSizeSum;

								int WaitMinutes = Convert.ToInt32(MeanWaitTime.TotalMinutes);
								string WaitTimeString;
								if (MeanWaitTime == TimeSpan.Zero)
								{
									WaitTimeString = "nil";
								}
								else if (MeanWaitTime < TimeSpan.FromMinutes(1))
								{
									WaitTimeString = "< 1 minute";
								}
								else if (WaitMinutes == 1)
								{
									WaitTimeString = "1 minute";
								}
								else
								{
									WaitTimeString = string.Format("{0} minutes", WaitMinutes);
								}
								StatusReportMessage.AppendLine("Queue waiting time " + WaitTimeString);
							}

							StatusReportMessage.AppendLine("Queue sizes...");
							StatusReportMessage.Append("> ");
							foreach (var Queue in QueueSizes)
							{
								StatusReportMessage.Append(Queue.Key + " " + Queue.Value + "                ");
							}
							StatusReportMessage.AppendLine();
						}
						if (CountsInPeriod.Count > 0)
						{
							string PeriodTimeString;
							if (InPeriod.TotalMinutes < 1.0)
							{
								PeriodTimeString = string.Format("{0:N0} seconds", InPeriod.TotalSeconds);
							}
							else
							{
								PeriodTimeString = string.Format("{0:N0} minutes", InPeriod.TotalMinutes);
							}

							StatusReportMessage.AppendLine(string.Format("Events in the last {0}...", PeriodTimeString));
							foreach (var CountInPeriod in CountsInPeriod)
							{
								if (CountInPeriod.Value > 0)
								{
									StatusReportMessage.AppendLine("> " + CountInPeriod.Key + " " + CountInPeriod.Value);
								}
							}
						}
					}
					return StatusReportMessage.ToString();
				}),

				new StatusReportLoop(TimeSpan.FromDays(1.0), InPeriod =>
				{
					StringBuilder DailyReportMessage = new StringBuilder();
					lock (DataLock)
					{
						DailyReportMessage.AppendLine("Disk space available...");

						foreach (var FolderMonitor in FolderMonitors)
						{
							string FreeSpaceText = "#Error";
							string Drive = FolderMonitor.Key;
							Int64 FreeSpace;
							float FreePercent;
							if (CrashReportCommon.StorageSpaceHelper.TryGetSpaceAvailable(Drive, out FreeSpace, out FreePercent))
							{
								FreeSpaceText = GetDiskSpaceString(FreeSpace, FreePercent);
							}

							DailyReportMessage.AppendLine("> " + FolderMonitor.Value + " =>> " + FreeSpaceText);
						}
					}
					return DailyReportMessage.ToString();
				}), 
			};
		}

		public void OnPreStopping()
		{
			CrashReporterProcessServicer.WriteSlack("CRP stopping...");
		}

		public void Dispose()
		{
			if (ReporterTasks != null)
			{
				foreach (var ReporterTask in ReporterTasks)
				{
					ReporterTask.Dispose();
				}
			}

			CrashReporterProcessServicer.WriteSlack("CRP stopped");
		}

		private static Dictionary<string, int> GetCountsInPeriod(Dictionary<string, int> InCounters, Dictionary<string, int> InCountersAtLastReport)
		{
			Dictionary<string, int> CountsInPeriod = new Dictionary<string, int>();

			foreach (var Counter in InCounters)
			{
				int LastCount = 0;
				if (!InCountersAtLastReport.TryGetValue(Counter.Key, out LastCount))
				{
					InCountersAtLastReport.Add(Counter.Key, 0);
				}
				CountsInPeriod.Add(Counter.Key, Counter.Value - LastCount);
				InCountersAtLastReport[Counter.Key] = Counter.Value;
			}

			return CountsInPeriod;
		}

		private static string GetDiskSpaceString(long FreeSpace, float FreePercent)
		{
			string FreeSpaceText;
			const long KB = 1 << 10;
			const long MB = 1 << 20;
			const long GB = 1 << 30;
			if (FreeSpace > GB)
			{
				FreeSpaceText = (FreeSpace / GB) + " GB";
			}
			else if (FreeSpace > MB)
			{
				FreeSpaceText = (FreeSpace / MB) + " MB";
			}
			else if (FreeSpace > KB)
			{
				FreeSpaceText = (FreeSpace / KB) + " KB";
			}
			else
			{
				FreeSpaceText = FreeSpace + " bytes";
			}

			FreeSpaceText += string.Format(" ({0:P1})", FreePercent*0.01f);
			return FreeSpaceText;
		}

		private bool CheckRecentAlerts(string AlertKey, int RepeatMinimumMinutes)
		{
			// Check RecentAlerts for a matching entry without the time period
			// that limits repeats Config.Default.SlackAlertRepeatMinimumMinutes
			if (RecentAlerts.ContainsKey(AlertKey))
			{
				DateTime LastMatchingAlert = RecentAlerts[AlertKey];
				TimeSpan RepeatMinimum = TimeSpan.FromMinutes(RepeatMinimumMinutes);

				if (LastMatchingAlert < DateTime.UtcNow - RepeatMinimum)
				{
					RecentAlerts[AlertKey] = DateTime.UtcNow;
					return true;
				}

				return false;
			}

			RecentAlerts.Add(AlertKey, DateTime.UtcNow);
			return true;
		}

		private IEnumerable<StatusReportLoop> ReporterTasks;
		private readonly Dictionary<string, string> QueueLocations = new Dictionary<string, string>();
		private readonly Dictionary<string, int> QueueSizes = new Dictionary<string, int>();
		private readonly Dictionary<string, int> Counters = new Dictionary<string, int>();
		private readonly Dictionary<string, string> FolderMonitors = new Dictionary<string, string>();
		private int QueueSizeSumAtLastReport = 0;
		private readonly Dictionary<string, int> CountersAtLastReport = new Dictionary<string, int>();
		private readonly Object DataLock = new Object();
		private int SetQueueSizesCallCount = 0;
		private readonly Dictionary<string, DateTime> RecentAlerts = new Dictionary<string, DateTime>();
	}
}
