//------------------------------------------------------------------------
// R2Project licence - the two things the panel shows
//
//   LicenceBadge    one small line in the panel's bottom-right corner:
//                   "Evaluation - 5 days left - licence..." or
//                   "Licensed to Andy Cobley". Click it for the dialog.
//
//   LicenceDialog   drawn INSIDE the panel, over it, as a VSTGUI modal
//                   view session - not a native window, so it behaves the
//                   same in every host, VST3 or AU, and can never appear
//                   when there is no editor (a host scan, auval).
//
// All the decisions and all the words that matter are in LicenceState.h,
// where they are tested; this file only lays them out. The design is the
// Nag-ware Licensing Design doc: the reminder thanks you, offers Get
// activation code (the website's free registration, LicenceConfig.h
// kActivationUrl), Enter code and Continue evaluating, and Continue works
// for ever.
//------------------------------------------------------------------------

#pragma once

#include "LicenceStore.h"
#include "SimpleSynthControls.h"

#include "vstgui/vstgui.h"

#include <functional>
#include <string>

namespace SimpleSynth {

/** True the first time it is called in this process, false after: the
    reminder shows at most once per session, however many instances or
    editor openings there are. */
bool claimSessionReminder ();

//------------------------------------------------------------------------
class LicenceBadge : public VSTGUI::CView
{
public:
	explicit LicenceBadge (const VSTGUI::CRect& size);

	void setDecision (const R2Licence::Decision& decision);
	void setHandler (std::function<void ()> handler) { mHandler = std::move (handler); }

	/** ADDED FOR THE UPDATED PANEL: draw the text in a rounded PILL,
	    right-aligned in the view - amber with a dot while evaluating,
	    mint once licensed - and take clicks only on the pill. Off (the
	    default) is the old one-line badge, unchanged. */
	void setPill (bool pill) { mPill = pill; invalid (); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;

private:
	std::string mText;
	bool mLicensed = false;
	bool mPressed = false;
	bool mPill = false;
	VSTGUI::CRect mPillRect;   // where the pill was last drawn
	std::function<void ()> mHandler;

	void drawPill (VSTGUI::CDrawContext* context);
	bool hit (const VSTGUI::CPoint& where) const;
};

//------------------------------------------------------------------------
class LicenceDialog : public VSTGUI::CViewContainer
{
public:
	enum class Page
	{
		Reminder,  // thanks, days used, Get activation code / Enter code / Continue evaluating
		Enter,     // name + code fields, Paste, Activate
		Licensed   // "Licensed to ...": the About box
	};

	/** `size` is the whole panel: the dialog dims it and takes every
	    click. `onClose` is called from a button handler - the owner must
	    end the modal session AFTER the event (CFrame::
	    doAfterEventProcessing), not from inside it. */
	LicenceDialog (const VSTGUI::CRect& size, R2Licence::LicenceStore& store,
	               std::function<void ()> onClose);

	void showPage (Page page);

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;

	/** Cmd+V (Ctrl+V on Windows), handled HERE rather than left to the
	    text field. On macOS a text field's paste comes from the host's
	    Edit menu, and Reaper's Edit > Paste is Reaper's own - so Cmd+V
	    in a plug-in's field pasted nothing. VSTGUI offers the key to the
	    frame first (performKeyEquivalent), so consuming it here wins. */
	void onKeyboardEvent (VSTGUI::KeyboardEvent& event) override;

private:
	VSTGUI::CTextLabel* addLabel (const VSTGUI::CRect& r, VSTGUI::CFontRef font, const VSTGUI::CColor& colour);
	SpyPresetButton* addButton (const char* name, std::function<void ()> handler);
	void placeButtons (std::initializer_list<SpyPresetButton*> buttons);
	void activate ();
	void pasteFromClipboard ();
	void pasteShortcut ();
	std::string clipboardText () const;
	bool pasteLicence (const std::string& text);  // false: no key in it
	Page mPage = Page::Reminder;
	std::string fieldText (VSTGUI::CTextEdit* field) const;

	R2Licence::LicenceStore& mStore;
	std::function<void ()> mOnClose;
	R2Licence::Decision mDecision;

	VSTGUI::CRect mBox;  // the dialog's own rectangle, centred in the panel

	VSTGUI::CTextLabel* mTitle = nullptr;
	VSTGUI::CTextLabel* mLine[3] = {};
	VSTGUI::CTextLabel* mNameLabel = nullptr;
	VSTGUI::CTextLabel* mKeyLabel = nullptr;
	VSTGUI::CTextEdit* mNameField = nullptr;
	VSTGUI::CTextEdit* mKeyField = nullptr;
	VSTGUI::CTextLabel* mStatus = nullptr;

	SpyPresetButton* mGetCodeButton = nullptr;
	SpyPresetButton* mEnterButton = nullptr;
	SpyPresetButton* mContinueButton = nullptr;
	SpyPresetButton* mPasteButton = nullptr;
	SpyPresetButton* mActivateButton = nullptr;
	SpyPresetButton* mCancelButton = nullptr;
	SpyPresetButton* mChangeButton = nullptr;
	SpyPresetButton* mCloseButton = nullptr;
	SpyPresetButton* mAllButtons[8] = {};
};

} // namespace SimpleSynth
