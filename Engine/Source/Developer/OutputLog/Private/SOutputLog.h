// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BaseTextLayoutMarshaller.h"
#include "TextFilterExpressionEvaluator.h"

class FOutputLogTextLayoutMarshaller;
class SSearchBox;

/**
* A single log message for the output log, holding a message and
* a style, for color and bolding of the message.
*/
struct FLogMessage
{
	TSharedRef<FString> Message;
	ELogVerbosity::Type Verbosity;
	FName Style;

	FLogMessage(const TSharedRef<FString>& NewMessage, FName NewStyle = NAME_None)
		: Message(NewMessage)
		, Verbosity(ELogVerbosity::Log)
		, Style(NewStyle)
	{
	}

	FLogMessage(const TSharedRef<FString>& NewMessage, ELogVerbosity::Type NewVerbosity, FName NewStyle = NAME_None)
		: Message(NewMessage)
		, Verbosity(NewVerbosity)
		, Style(NewStyle)
	{
	}
};

/**
 * Console input box with command-completion support
 */
class SConsoleInputBox
	: public SCompoundWidget
{

public:
	DECLARE_DELEGATE_OneParam(FExecuteConsoleCommand, const FString& /*ExecCommand*/)

	SLATE_BEGIN_ARGS( SConsoleInputBox )
		: _SuggestionListPlacement( MenuPlacement_BelowAnchor )
		{}

		/** Where to place the suggestion list */
		SLATE_ARGUMENT( EMenuPlacement, SuggestionListPlacement )

		/** Custom executor for console command, will be used when bound */
		SLATE_EVENT( FExecuteConsoleCommand, ConsoleCommandCustomExec)

		/** Called when a console command is executed */
		SLATE_EVENT( FSimpleDelegate, OnConsoleCommandExecuted )
	SLATE_END_ARGS()

	/** Protected console input box widget constructor, called by Slate */
	SConsoleInputBox();

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs	Declaration used by the SNew() macro to construct this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Returns the editable text box associated with this widget.  Used to set focus directly. */
	TSharedRef< SEditableTextBox > GetEditableTextBox()
	{
		return InputText.ToSharedRef();
	}

	/** SWidget interface */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	virtual bool SupportsKeyboardFocus() const override { return true; }

	// e.g. Tab or Key_Up
	virtual FReply OnPreviewKeyDown( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent ) override;

	void OnFocusLost( const FFocusEvent& InFocusEvent ) override;

	/** Handles entering in a command */
	void OnTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	void OnTextChanged(const FText& InText);

	/** Makes the widget for the suggestions messages in the list view */
	TSharedRef<ITableRow> MakeSuggestionListItemWidget(TSharedPtr<FString> Message, const TSharedRef<STableViewBase>& OwnerTable);

	void SuggestionSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
		
	void SetSuggestions(TArray<FString>& Elements, bool bInHistoryMode);

	void MarkActiveSuggestion();

	void ClearSuggestions();

	FString GetSelectionText() const;

private:

	/** Editable text widget */
	TSharedPtr< SEditableTextBox > InputText;

	/** history / auto completion elements */
	TSharedPtr< SMenuAnchor > SuggestionBox;

	/** All log messages stored in this widget for the list view */
	TArray< TSharedPtr<FString> > Suggestions;

	/** The list view for showing all log messages. Should be replaced by a full text editor */
	TSharedPtr< SListView< TSharedPtr<FString> > > SuggestionListView;

	/** Delegate to call when a console command is executed */
	FSimpleDelegate OnConsoleCommandExecuted;

	/** Delegate to call to execute console command */
	FExecuteConsoleCommand ConsoleCommandCustomExec;

	/** -1 if not set, otherwise index into Suggestions */
	int32 SelectedSuggestion;

	/** to prevent recursive calls in UI callback */
	bool bIgnoreUIUpdate; 
};

/**
* Holds information about filters
*/
struct FLogFilter
{
	/** true to show Logs. */
	bool bShowLogs;

	/** true to show Warnings. */
	bool bShowWarnings;

	/** true to show Errors. */
	bool bShowErrors;

	/** Enable all filters by default */
	FLogFilter() : TextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::BasicString)
	{
		bShowErrors = bShowLogs = bShowWarnings = true;
	}

	/** Returns true if any messages should be filtered out */
	bool IsFilterSet() { return !bShowErrors || !bShowLogs || !bShowWarnings || TextFilterExpressionEvaluator.GetFilterType() != ETextFilterExpressionType::Empty || !TextFilterExpressionEvaluator.GetFilterText().IsEmpty(); }

	/** Checks the given message against set filters */
	bool IsMessageAllowed(const TSharedPtr<FLogMessage>& Message);

	/** Set the Text to be used as the Filter's restrictions */
	void SetFilterText(const FText& InFilterText) { TextFilterExpressionEvaluator.SetFilterText(InFilterText); }

	/** Returns Evaluator syntax errors (if any) */
	FText GetSyntaxErrors() { return TextFilterExpressionEvaluator.GetFilterErrorText(); }

private:
	/** Expression evaluator that can be used to perform complex text filter queries */
	FTextFilterExpressionEvaluator TextFilterExpressionEvaluator;
};

/**
 * Widget which holds a list view of logs of the program output
 * as well as a combo box for entering in new commands
 */
class SOutputLog 
	: public SCompoundWidget, public FOutputDevice
{

public:

	SLATE_BEGIN_ARGS( SOutputLog )
		: _Messages()
		{}
		
		/** All messages captured before this log window has been created */
		SLATE_ARGUMENT( TArray< TSharedPtr<FLogMessage> >, Messages )

	SLATE_END_ARGS()

	/** Destructor for output log, so we can unregister from notifications */
	~SOutputLog();

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs	Declaration used by the SNew() macro to construct this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Creates FLogMessage objects from FOutputDevice log callback
	 *
	 * @param	V Message text
	 * @param Verbosity Message verbosity
	 * @param Category Message category
	 * @param OutMessages Array to receive created FLogMessage messages
	 * @param Filters [Optional] Filters to apply to Messages
	 *
	 * @return true if any messages have been created, false otherwise
	 */
	static bool CreateLogMessages(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category, TArray< TSharedPtr<FLogMessage> >& OutMessages);

protected:

	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

protected:
	/**
	 * Extends the context menu used by the text box
	 */
	void ExtendTextBoxMenu(FMenuBuilder& Builder);

	/**
	 * Called when delete all is selected
	 */
	void OnClearLog();

	/**
	 * Called when the user scrolls the log window vertically
	 */
	void OnUserScrolled(float ScrollOffset);

	/**
	 * Called to determine whether delete all is currently a valid command
	 */
	bool CanClearLog() const;

	/** Called when a console command is entered for this output log */
	void OnConsoleCommandExecuted();

	/** Request we immediately force scroll to the bottom of the log */
	void RequestForceScroll();

	/** Converts the array of messages into something the text box understands */
	TSharedPtr< FOutputLogTextLayoutMarshaller > MessagesTextMarshaller;

	/** The editable text showing all log messages */
	TSharedPtr< SMultiLineEditableTextBox > MessagesTextBox;

	/** The editable text showing all log messages */
	TSharedPtr< SSearchBox > FilterTextBox;

	/** True if the user has scrolled the window upwards */
	bool bIsUserScrolled;

private:
	/** Called by Slate when the filter box changes text. */
	void OnFilterTextChanged(const FText& InFilterText);

	/** Make the "Filters" menu. */
	TSharedRef<SWidget> MakeAddFilterMenu();

	/** Fills in the filter menu. */
	void FillVerbosityEntries(FMenuBuilder& MenuBuilder);

	/** A simple function for the filters to keep them enabled. */
	bool Menu_CanExecute() const;

	/** Toggles "Logs" true/false. */
	void MenuLogs_Execute();

	/** Returns the state of "Logs". */
	bool MenuLogs_IsChecked() const;

	/** Toggles "Warnings" true/false. */
	void MenuWarnings_Execute();

	/** Returns the state of "Warnings". */
	bool MenuWarnings_IsChecked() const;

	/** Toggles "Errors" true/false. */
	void MenuErrors_Execute();

	/** Returns the state of "Errors". */
	bool MenuErrors_IsChecked() const;

	/** Forces re-population of the messages list */
	void Refresh();

public:
	/** Visible messages filter */
	FLogFilter Filter;
};

/** Output log text marshaller to convert an array of FLogMessages into styled lines to be consumed by an FTextLayout */
class FOutputLogTextLayoutMarshaller : public FBaseTextLayoutMarshaller
{
public:

	static TSharedRef< FOutputLogTextLayoutMarshaller > Create(TArray< TSharedPtr<FLogMessage> > InMessages, FLogFilter* InFilter);

	virtual ~FOutputLogTextLayoutMarshaller();
	
	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;

	bool AppendMessage(const TCHAR* InText, const ELogVerbosity::Type InVerbosity, const FName& InCategory);
	void ClearMessages();

	void CountMessages();

	int32 GetNumMessages() const;
	int32 GetNumFilteredMessages();

	void MarkMessagesCacheAsDirty();

protected:

	FOutputLogTextLayoutMarshaller(TArray< TSharedPtr<FLogMessage> > InMessages, FLogFilter* InFilter);

	void AppendMessageToTextLayout(const TSharedPtr<FLogMessage>& InMessage);
	void AppendMessagesToTextLayout(const TArray<TSharedPtr<FLogMessage>>& InMessages);

	/** All log messages to show in the text box */
	TArray< TSharedPtr<FLogMessage> > Messages;

	/** Holds cached numbers of messages to avoid unnecessary re-filtering */
	int32 CachedNumMessages;
	
	/** Flag indicating the messages count cache needs rebuilding */
	bool bNumMessagesCacheDirty;

	/** Visible messages filter */
	FLogFilter* Filter;

	FTextLayout* TextLayout;
};
