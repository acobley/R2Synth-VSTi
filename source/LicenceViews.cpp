//------------------------------------------------------------------------
// R2Project licence - the badge and the dialog. See LicenceViews.h.
//------------------------------------------------------------------------

// Opening the activation page in the browser. Before anything else is
// included: CoreServices pulls in AssertMacros.h, whose un-prefixed
// check()/verify()/require() macros would otherwise be defined.
#if defined(__APPLE__)
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#include "LicenceViews.h"
#include "R2SynthPanelViews.h"
#include "LicenceConfig.h"
#include "LicencePublicKey.h"

#include <algorithm>
#include <atomic>
#include <cstring>

using namespace VSTGUI;

namespace R2Synth {

//------------------------------------------------------------------------
namespace {

std::atomic<bool> gReminderShown {false};

const CColor kDim (0, 0, 0, 150);           // over the panel behind the dialog
const CColor kBoxBack (58, 63, 58, 255);    // a shade up from the panel
const CColor kFieldBack (24, 26, 24, 255);
const CColor kGood (50, 255, 50, 255);      // the panel's green
const CColor kBad (255, 110, 110, 255);

constexpr CCoord kBoxWidth = 620.;
constexpr CCoord kBoxHeight = 214.;
constexpr CCoord kPad = 16.;
constexpr CCoord kLineHeight = 16.;
constexpr CCoord kButtonWidth = 150.;
constexpr CCoord kButtonHeight = 26.;
constexpr CCoord kButtonGap = 8.;

CFontRef titleFont ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Arial", 14, kBoldFace);
	return font;
}

/** Draws a frame round the dialog and fills it; mouse-transparent. */
class DialogBox : public CView
{
public:
	using CView::CView;
	void draw (CDrawContext* context) override
	{
		const CRect r = getViewSize ();
		context->setFillColor (kBoxBack);
		context->drawRect (r, kDrawFilled);
		context->setFrameColor (Colours::kGroupFrame);
		context->setLineWidth (1.);
		CRect edge (r);
		edge.inset (0.5, 0.5);
		context->drawRect (edge, kDrawStroked);
		setDirty (false);
	}
};

std::string plural (int n, const char* one, const char* many)
{
	return std::to_string (n) + " " + (n == 1 ? one : many);
}

/** Opens `url` in the default browser. Launch Services on macOS - allowed
    from a sandboxed AU host too; no shell, no NSWorkspace (this is C++). */
bool openInBrowser (const char* url)
{
#if defined(__APPLE__)
	CFURLRef u = CFURLCreateWithBytes (nullptr, reinterpret_cast<const UInt8*> (url),
	                                   static_cast<CFIndex> (std::strlen (url)), kCFStringEncodingUTF8, nullptr);
	if (!u)
		return false;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	const OSStatus status = LSOpenCFURLRef (u, nullptr);
#pragma clang diagnostic pop
	CFRelease (u);
	return status == noErr;
#elif defined(_WIN32)
	return reinterpret_cast<INT_PTR> (ShellExecuteA (nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#else
	(void)url;
	return false;
#endif
}

} // namespace

bool claimSessionReminder ()
{
	return !gReminderShown.exchange (true);
}

//------------------------------------------------------------------------
// LicenceBadge
//------------------------------------------------------------------------
LicenceBadge::LicenceBadge (const CRect& size)
: CView (size)
{
}

void LicenceBadge::setDecision (const R2Licence::Decision& decision)
{
	mText = R2Licence::badgeText (decision);
	mLicensed = decision.licensed;
	invalid ();
}

bool LicenceBadge::hit (const CPoint& where) const
{
	return mPill ? mPillRect.pointInside (where) : getViewSize ().pointInside (where);
}

void LicenceBadge::drawPill (CDrawContext* context)
{
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	context->setFont (Panel::fontTitle ());
	const CCoord textWidth = context->getStringWidth (mText.c_str ());
	const CCoord height = r.getHeight ();
	const CCoord width = std::min (r.getWidth (), textWidth + 40.);
	mPillRect = CRect (r.right - width, r.top, r.right, r.bottom);

	const CColor back   = mLicensed ? CColor (23, 39, 31, 255)   : CColor (42, 36, 24, 255);
	const CColor edge   = mLicensed ? CColor (43, 74, 58, 255)   : CColor (74, 59, 30, 255);
	const CColor ink    = mLicensed ? CColor (143, 224, 187, 255) : CColor (242, 194, 122, 255);
	const CColor dotInk = Panel::Palette::accent (mLicensed ? Layout::Accent::Amp : Layout::Accent::Osc);

	auto path = owned (context->createGraphicsPath ());
	if (path)
	{
		CRect pill (mPillRect);
		pill.inset (0.5, 0.5);
		path->addRoundRect (pill, height / 2.);
		context->setFillColor (back);
		context->drawGraphicsPath (path, CDrawContext::kPathFilled);
		context->setFrameColor (edge);
		context->setLineWidth (1.);
		context->setLineStyle (kLineSolid);
		context->drawGraphicsPath (path, CDrawContext::kPathStroked);
	}

	const CPoint dot (mPillRect.left + 16., (r.top + r.bottom) / 2.);
	context->setFillColor (dotInk);
	context->drawEllipse (CRect (dot.x - 4., dot.y - 4., dot.x + 4., dot.y + 4.), kDrawFilled);

	context->setFontColor (mPressed ? Panel::Palette::kText : ink);
	CRect text (mPillRect);
	text.left += 28.;
	context->drawString (mText.c_str (), text, kLeftText, true);
	setDirty (false);
}

void LicenceBadge::draw (CDrawContext* context)
{
	if (mPill)
	{
		drawPill (context);
		return;
	}

	const CRect r = getViewSize ();
	context->setFont (panelFontTiny ());
	context->setFontColor (mPressed ? Colours::kValue : (mLicensed ? Colours::kLabel : Colours::kGroupFrame));
	context->drawString (mText.c_str (), r, kRightText, true);
	setDirty (false);
}

void LicenceBadge::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft () || !hit (event.mousePosition))
		return;
	mPressed = true;
	invalid ();
	event.consumed = true;
}

void LicenceBadge::onMouseUpEvent (MouseUpEvent& event)
{
	if (!mPressed)
		return;
	mPressed = false;
	invalid ();
	event.consumed = true;
	if (hit (event.mousePosition) && mHandler)
		mHandler ();  // last: the handler opens a modal session
}

void LicenceBadge::onMouseCancelEvent (MouseCancelEvent& event)
{
	mPressed = false;
	invalid ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// LicenceDialog
//------------------------------------------------------------------------
LicenceDialog::LicenceDialog (const CRect& size, R2Licence::LicenceStore& store,
                              std::function<void ()> onClose)
: CViewContainer (size)
, mStore (store)
, mOnClose (std::move (onClose))
{
	setBackgroundColor (kDim);

	const CCoord left = (size.getWidth () - kBoxWidth) / 2.;
	const CCoord top = (size.getHeight () - kBoxHeight) / 2.;
	mBox = CRect (left, top, left + kBoxWidth, top + kBoxHeight);

	auto* box = new DialogBox (mBox);
	box->setMouseEnabled (false);
	addView (box);

	const CCoord x = mBox.left + kPad;
	const CCoord w = kBoxWidth - 2 * kPad;
	CCoord y = mBox.top + 12.;

	mTitle = addLabel (CRect (x, y, x + w, y + 20.), titleFont (), Colours::kValue);
	y += 28.;
	for (auto*& line : mLine)
	{
		line = addLabel (CRect (x, y, x + w, y + kLineHeight), panelFont (), Colours::kValue);
		y += kLineHeight + 2.;
	}

	// The Enter page's fields sit under its one line of instructions.
	const CCoord labelW = 44.;
	const CCoord fx = x + labelW;
	const CCoord fw = w - labelW;
	CCoord fy = mBox.top + 12. + 28. + kLineHeight + 10.;
	mNameLabel = addLabel (CRect (x, fy + 2., fx, fy + 18.), panelFont (), Colours::kLabel);
	mNameLabel->setText ("Name");
	mNameField = new CTextEdit (CRect (fx, fy, fx + fw, fy + 20.), nullptr, -1);
	fy += 28.;
	mKeyLabel = addLabel (CRect (x, fy + 2., fx, fy + 18.), panelFont (), Colours::kLabel);
	mKeyLabel->setText ("Code");
	mKeyField = new CTextEdit (CRect (fx, fy, fx + fw, fy + 20.), nullptr, -1);
	for (auto* field : {mNameField, mKeyField})
	{
		field->setFont (field == mKeyField ? panelFontSmall () : panelFont ());
		field->setFontColor (Colours::kValue);
		field->setBackColor (kFieldBack);
		field->setFrameColor (Colours::kGroupFrame);
		field->setHoriAlign (kLeftText);
		field->setTextInset (CPoint (4., 0.));
		field->setImmediateTextChange (true);
		addView (field);
	}
	mNameField->setPlaceholderString ("Your name, exactly as on the activation page");
	mKeyField->setPlaceholderString ("BNDL1-...");
	fy += 30.;
	mStatus = addLabel (CRect (x, fy, x + w, fy + kLineHeight), panelFont (), kBad);

	mGetCodeButton = addButton ("Get activation code", [this] {
		if (!openInBrowser (R2Licence::kActivationUrl))
		{
			mLine[2]->setText ((std::string ("Couldn't open the browser - go to ") + R2Licence::kActivationUrl).c_str ());
			mLine[2]->setFontColor (kBad);
			mLine[2]->invalid ();
		}
	});
	mEnterButton = addButton ("Enter code", [this] { showPage (Page::Enter); });
	mContinueButton = addButton ("Continue evaluating", [this] { if (mOnClose) mOnClose (); });
	mPasteButton = addButton ("Paste", [this] { pasteFromClipboard (); });
	mActivateButton = addButton ("Activate", [this] { activate (); });
	mCancelButton = addButton ("Cancel", [this] { if (mOnClose) mOnClose (); });
	mChangeButton = addButton ("Change code", [this] { showPage (Page::Enter); });
	mCloseButton = addButton ("Close", [this] { if (mOnClose) mOnClose (); });
}

//------------------------------------------------------------------------
CTextLabel* LicenceDialog::addLabel (const CRect& r, CFontRef font, const CColor& colour)
{
	auto* label = new CTextLabel (r, "");
	label->setFont (font);
	label->setFontColor (colour);
	label->setBackColor (kBoxBack);
	label->setFrameColor (kBoxBack);
	label->setHoriAlign (kLeftText);
	label->setMouseEnabled (false);
	addView (label);
	return label;
}

SpyPresetButton* LicenceDialog::addButton (const char* name, std::function<void ()> handler)
{
	auto* button = new SpyPresetButton (CRect (0, 0, kButtonWidth, kButtonHeight), name);
	button->setHandler (std::move (handler));
	button->setVisible (false);
	addView (button);
	for (auto*& slot : mAllButtons)
		if (!slot)
		{
			slot = button;
			break;
		}
	return button;
}

void LicenceDialog::placeButtons (std::initializer_list<SpyPresetButton*> buttons)
{
	for (auto* b : mAllButtons)
		if (b)
			b->setVisible (false);

	// Right-aligned along the bottom, in the order given.
	CCoord right = mBox.right - kPad;
	const CCoord top = mBox.bottom - kPad - kButtonHeight;
	for (auto it = std::rbegin (buttons); it != std::rend (buttons); ++it)
	{
		(*it)->setViewSize (CRect (right - kButtonWidth, top, right, top + kButtonHeight));
		(*it)->setVisible (true);
		right -= kButtonWidth + kButtonGap;
	}
}

//------------------------------------------------------------------------
void LicenceDialog::showPage (Page page)
{
	mPage = page;
	mDecision = mStore.current (R2Licence::LicenceStore::today ());
	const bool enter = (page == Page::Enter);
	for (auto* v : {static_cast<CView*> (mNameLabel), static_cast<CView*> (mKeyLabel),
	                static_cast<CView*> (mNameField), static_cast<CView*> (mKeyField)})
		v->setVisible (enter);
	mStatus->setText ("");
	mStatus->setVisible (enter);
	for (auto* line : mLine)
	{
		line->setText ("");
		line->setFontColor (Colours::kValue);
	}
	// The Enter page has one line of instructions; its fields sit where
	// the other two lines would be.
	mLine[1]->setVisible (!enter);
	mLine[2]->setVisible (!enter);

	switch (page)
	{
		case Page::Reminder:
		{
			const auto& d = mDecision;
			if (d.outOfScope)
			{
				mTitle->setText ("Your licence covers an earlier version");
				mLine[0]->setText (("The licence for " + d.licensedName +
				                    " doesn't include this version of R2Synth.").c_str ());
				mLine[1]->setText ("R2Synth keeps working in full while you decide - there's no time limit.");
				mLine[2]->setText ("Upgrade, or enter a licence that covers this version.");
			}
			else
			{
				mTitle->setText ("Thank you for trying R2Synth");
				mLine[0]->setText ("It's free to try and keeps working in full - there's no time limit.");
				mLine[1]->setText ("A free activation code turns this reminder off - it takes your name and email.");
				const std::string when =
				    d.reminderDue ? "You've been evaluating it for " + plural (d.daysUsed, "day", "days") + "."
				                  : (d.daysLeft == 0 ? std::string ("This is the last day of the evaluation.")
				                                     : plural (d.daysLeft, "day", "days") + " of the evaluation left.");
				mLine[2]->setText (when.c_str ());
			}
			if (*R2Licence::kActivationUrl)
				placeButtons ({mGetCodeButton, mEnterButton, mContinueButton});
			else
				placeButtons ({mEnterButton, mContinueButton});
			break;
		}

		case Page::Enter:
			mTitle->setText ("Enter your activation code");
			mLine[0]->setText ("Copy your name and code from the activation page and press Paste, or type them.");
			if (mDecision.licensed || mDecision.outOfScope)
				mNameField->setText (mDecision.licensedName.c_str ());
			placeButtons ({mPasteButton, mActivateButton, mCancelButton});
			break;

		case Page::Licensed:
		{
			const auto& d = mDecision;
			mTitle->setText ("R2Synth is activated");
			mLine[0]->setText (("Licensed to " + d.licensedName).c_str ());
			std::string detail = "Licence " + std::to_string (d.payload.licenceId) + "  -  issued " +
			                     R2Licence::formatDate (d.payload.issueDay);
			if (R2Licence::kPublicKeyIsDevelopment)
				detail += "  -  DEVELOPMENT KEY";
			mLine[1]->setText (detail.c_str ());
			mLine[2]->setText (d.payload.product == R2Licence::kProductR2SynthFree
			                       ? "Thank you for registering."
			                       : "Thank you for supporting R2Project.");
			placeButtons ({mChangeButton, mCloseButton});
			break;
		}
	}
	invalid ();
}

//------------------------------------------------------------------------
std::string LicenceDialog::fieldText (CTextEdit* field) const
{
	return field->getText ().getString ();
}

void LicenceDialog::activate ()
{
	// Commit whatever is still being typed in a field.
	if (auto* f = getFrame ())
		f->setFocusView (nullptr);

	bool saved = false;
	const auto status = mStore.activate (fieldText (mNameField), fieldText (mKeyField), saved);
	if (status != R2Licence::Status::Valid)
	{
		mStatus->setFontColor (kBad);
		mStatus->setText (R2Licence::activationMessage (status).c_str ());
		mStatus->invalid ();
		return;
	}

	showPage (Page::Licensed);
	if (!saved)
	{
		mLine[2]->setText ("Licensed for this session - it couldn't be saved to disk.");
		mLine[2]->setFontColor (kBad);
	}
}

std::string LicenceDialog::clipboardText () const
{
	auto* frame = getFrame ();
	if (!frame)
		return std::string ();
	std::string text;
	if (auto pkg = frame->getClipboard ())
	{
		for (uint32_t i = 0; i < pkg->getCount (); ++i)
		{
			const void* data = nullptr;
			IDataPackage::Type type;
			const uint32_t size = pkg->getData (i, data, type);
			if (type == IDataPackage::kText && data && size)
			{
				text.assign (static_cast<const char*> (data), size);
				while (!text.empty () && text.back () == '\0')
					text.pop_back ();
				break;
			}
		}
	}
	return text;
}

void LicenceDialog::pasteFromClipboard ()
{
	if (!pasteLicence (clipboardText ()))
	{
		mStatus->setFontColor (kBad);
		mStatus->setText ("There's no BNDL1-... code on the clipboard. Copy it from the activation page first.");
		mStatus->invalid ();
	}
}

bool LicenceDialog::pasteLicence (const std::string& text)
{
	std::string name, key;
	if (!R2Licence::extractFromPaste (text, name, key))
		return false;
	if (auto* frame = getFrame ())
		frame->setFocusView (nullptr);
	if (!name.empty ())
		mNameField->setText (name.c_str ());
	mKeyField->setText (key.c_str ());
	mStatus->setFontColor (kGood);
	mStatus->setText (name.empty () ? "Code pasted - check the name, then Activate."
	                                : "Name and key pasted - press Activate.");
	mNameField->invalid ();
	mKeyField->invalid ();
	mStatus->invalid ();
	return true;
}

//------------------------------------------------------------------------
void LicenceDialog::onKeyboardEvent (KeyboardEvent& event)
{
	const bool pasteKey = event.type == EventType::KeyDown && event.modifiers.is (ModifierKey::Control) &&
	                      (event.character == 'v' || event.character == 'V');
	if (!pasteKey)
	{
		CViewContainer::onKeyboardEvent (event);
		return;
	}
	pasteShortcut ();
	event.consumed = true;  // so the host's Edit > Paste never sees it
}

void LicenceDialog::pasteShortcut ()
{
	const std::string text = clipboardText ();

	// A key on the clipboard - the key alone or the whole email - goes
	// where it belongs whichever field has the cursor, from any page.
	std::string name, key;
	if (R2Licence::extractFromPaste (text, name, key))
	{
		if (mPage != Page::Enter)
			showPage (Page::Enter);
		pasteLicence (text);
		return;
	}
	if (mPage != Page::Enter)
		return;

	// Anything else - a name - goes into the field with the cursor.
	auto* frame = getFrame ();
	CView* focus = frame ? frame->getFocusView () : nullptr;
	CTextEdit* field = (focus == mNameField) ? mNameField : (focus == mKeyField) ? mKeyField : nullptr;
	if (!field || text.empty ())
	{
		mStatus->setFontColor (kBad);
		mStatus->setText (text.empty () ? "The clipboard is empty." : "Click in the Name or Key box first, then paste.");
		mStatus->invalid ();
		return;
	}
	field->setText (R2Licence::pasteIntoField (fieldText (field), text).c_str ());
	field->invalid ();
}

//------------------------------------------------------------------------
void LicenceDialog::onMouseDownEvent (MouseDownEvent& event)
{
	CViewContainer::onMouseDownEvent (event);
	// Modal: a click anywhere, on the dimmed panel included, stops here.
	event.consumed = true;
}

} // namespace R2Synth
