#include "../iuidescwindow.h"
#include "../iwindowcontroller.h"
#include "../iapplication.h"
#include "../iappdelegate.h"
#include "../ialertbox.h"
#include "../../lib/iviewlistener.h"
#include "../../lib/cframe.h"
#include "../../lib/crect.h"
#include "../../lib/cfileselector.h"
#include "../../lib/cvstguitimer.h"
#include "../../lib/controls/coptionmenu.h"
#include "../../lib/controls/ctextedit.h"
#include "../../lib/controls/csegmentbutton.h"
#include "../../uidescription/uidescription.h"
#include "../../uidescription/uiattributes.h"
#include "../../uidescription/cstream.h"
#include "../../uidescription/detail/uiviewcreatorattributes.h"
#include "../../uidescription/editing/uieditcontroller.h"
#include "../../uidescription/editing/uieditmenucontroller.h"

//------------------------------------------------------------------------
namespace VSTGUI {
namespace Standalone {
namespace /* Anonymous */ {

using UIDesc::ModelBindingPtr;
using UIDesc::CustomizationPtr;

#if VSTGUI_LIVE_EDITING
//------------------------------------------------------------------------
struct EditFileMap
{
private:
	using Map = std::unordered_map<std::string, std::string>;
	Map fileMap;
	static EditFileMap& instance ()
	{
		static EditFileMap gInstance;
		return gInstance;
	}
public:
	static void set (const std::string& filename, const std::string& absolutePath)
	{
		instance ().fileMap.insert ({filename, absolutePath});
	}
	static const std::string& get (const std::string& filename)
	{
		auto it = instance ().fileMap.find (filename);
		if (it == instance ().fileMap.end ())
		{
			static std::string empty;
			return empty;
		}
		return it->second;
	}
};

//------------------------------------------------------------------------
bool initUIDescAsNew (UIDescription& uiDesc, CFrame* _frame)
{
	SharedPointer<CFrame> frame (_frame);
	if (!frame)
		frame = owned (new CFrame ({0, 0, 0, 0}, nullptr));
	auto fs = owned (CNewFileSelector::create (frame, CNewFileSelector::kSelectSaveFile));
	fs->setDefaultSaveName (uiDesc.getFilePath ());
	fs->setDefaultExtension (CFileExtension ("UIDescription File", "uidesc"));
	fs->setTitle ("Save UIDescription File");
	if (fs->runModal ())
	{
		if (fs->getNumSelectedFiles () == 0)
		{
			return false;
		}
		auto path = fs->getSelectedFile (0);
		uiDesc.setFilePath (path);
		auto settings = uiDesc.getCustomAttributes ("UIDescFilePath", true);
		settings->setAttribute ("path", path);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
enum class UIDescCheckFilePathResult
{
	exists,
	newPathSet,
	cancel
};

//------------------------------------------------------------------------
UIDescCheckFilePathResult checkAndUpdateUIDescFilePath (UIDescription& uiDesc, CFrame* _frame, UTF8StringPtr notFoundText = "The uidesc file location cannot be found.")
{
	SharedPointer<CFrame> frame (_frame);
	if (!frame)
		frame = owned (new CFrame ({0, 0, 0, 0}, nullptr));

	CFileStream stream;
	if (stream.open (uiDesc.getFilePath (), CFileStream::kReadMode))
		return UIDescCheckFilePathResult::exists;
	AlertBoxConfig alertConfig;
	alertConfig.headline = notFoundText;
	alertConfig.description = uiDesc.getFilePath ();
	alertConfig.defaultButton = "Locate";
	alertConfig.secondButton = "Close";
	auto alertResult = IApplication::instance ().showAlertBox (alertConfig);
	if (alertResult == AlertResult::secondButton)
	{
		return UIDescCheckFilePathResult::cancel;
	}
	auto fs = owned (CNewFileSelector::create (frame, CNewFileSelector::kSelectFile));
	fs->setDefaultExtension (CFileExtension ("UIDescription File", "uidesc"));
	if (fs->runModal ())
	{
		if (fs->getNumSelectedFiles () == 0)
		{
			return UIDescCheckFilePathResult::cancel;
		}
		uiDesc.setFilePath (fs->getSelectedFile (0));
		auto settings = uiDesc.getCustomAttributes ("UIDescFilePath", true);
		settings->setAttribute ("path", uiDesc.getFilePath ());
		return UIDescCheckFilePathResult::newPathSet;
	}
	return UIDescCheckFilePathResult::cancel;
}

#endif

//------------------------------------------------------------------------
struct SharedResources
{
	static SharedResources& instance ()
	{
		static SharedResources gInstance;
		return gInstance;
	}
	
	const SharedPointer<UIDescription>& get ()
	{
		load ();
		return uiDesc;
	}
	
	void unuse ()
	{
		save ();
		if (uiDesc && uiDesc->getNbReference () == 1)
			uiDesc = nullptr;
	}

private:
	bool load ()
	{
		if (uiDesc)
			return true;
		auto filename = IApplication::instance ().getDelegate ().getSharedUIResourceFilename ();
		if (!filename)
			return true;

#if VSTGUI_LIVE_EDITING
		auto& absPath = EditFileMap::get (filename);
		if (!absPath.empty ())
			filename = absPath.data ();
#endif

		uiDesc = owned (new UIDescription (filename));
		if (!uiDesc->parse ())
		{
#if VSTGUI_LIVE_EDITING
			if (!initUIDescAsNew (*uiDesc, nullptr))
				return false;
			else
#endif
			return false;
		}
		auto settings = uiDesc->getCustomAttributes ("UIDescFilePath", true);
		auto filePath = settings->getAttributeValue ("path");
		if (filePath)
			uiDesc->setFilePath (filePath->data ());

#if VSTGUI_LIVE_EDITING
		checkAndUpdateUIDescFilePath (*uiDesc, nullptr, "The resource ui desc file location cannot be found.");
		EditFileMap::set (IApplication::instance ().getDelegate ().getSharedUIResourceFilename (), uiDesc->getFilePath ());
#endif
		return true;
	}
	
	bool save ()
	{
#if VSTGUI_LIVE_EDITING
		if (uiDesc == nullptr)
			return true;

		if (uiDesc->save (uiDesc->getFilePath (), UIDescription::kWriteImagesIntoXMLFile))
			return true;
		AlertBoxConfig config;
		config.headline = "Saving the shared resources uidesc file failed.";
		IApplication::instance ().showAlertBox (config);
		return false;
#else
		return true;
#endif
	}
	SharedPointer<UIDescription> uiDesc;
};

//------------------------------------------------------------------------
class WindowController : public WindowControllerAdapter, public ICommandHandler
{
public:
	bool init (const UIDesc::Config& config, WindowPtr& window);
	
	CPoint constraintSize (const IWindow& window, const CPoint& newSize) override;
	void onClosed (const IWindow& window) override;
	bool canClose (const IWindow& window) const override;
	
	bool canHandleCommand (const Command& command) override;
	bool handleCommand (const Command& command) override;
private:
	
	struct Impl;
	struct EditImpl;
	std::unique_ptr<Impl> impl;
};

//------------------------------------------------------------------------
class ValueWrapper : public ValueListenerAdapter, public IControlListener, public IViewListenerAdapter
{
public:
	ValueWrapper (const ValuePtr& value = nullptr)
	: value (value)
	{
		if (value)
			value->registerListener (this);
	}
	~ValueWrapper ()
	{
		if (value)
			value->unregisterListener (this);
		for (auto& control : controls)
		{
			control->unregisterViewListener (this);
			control->unregisterControlListener (this);
		}
	}

	const UTF8String& getID () const { return value->getID (); }
	
	void onPerformEdit (const IValue& value, IValue::Type newValue) override
	{
		for (auto& c : controls)
		{
			c->setValueNormalized (static_cast<float> (newValue));
			c->invalid ();
		}
	}

	void updateControlOnStateChange (CControl* control) const
	{
		control->setMin (0.f);
		control->setMax (1.f);
		control->setMouseEnabled (value->isActive ());
		auto stepValue = value->dynamicCast<const IStepValue> ();
		if (!stepValue)
			return;
		if (auto menu = dynamic_cast<COptionMenu*>(control))
		{
			menu->removeAllEntry ();
			for (IStepValue::StepType i = 0; i < stepValue->getSteps (); ++i)
			{
				auto title = value->getStringConverter ().valueAsString (stepValue->stepToValue (i));
				menu->addEntry (title);
			}
		}
		if (auto segmentButton = dynamic_cast<CSegmentButton*>(control))
		{
			segmentButton->removeAllSegments ();
			for (IStepValue::StepType i = 0; i < stepValue->getSteps (); ++i)
			{
				auto title = value->getStringConverter ().valueAsString (stepValue->stepToValue (i));
				segmentButton->addSegment ({title});
			}
		}
	}

	void onStateChange (const IValue& value) override
	{
		for (auto& c : controls)
		{
			updateControlOnStateChange (c);
		}
	}

	void valueChanged (CControl* control) override
	{
		auto preValue = value->getValue ();
		value->performEdit (control->getValueNormalized ());
		if (value->getValue () == preValue)
			onPerformEdit (*value, value->getValue ());
	}

	void controlBeginEdit (CControl* control) override { value->beginEdit (); }
	void controlEndEdit (CControl* control) override { value->endEdit (); }

	void addControl (CControl* control)
	{
		if (auto paramDisplay = dynamic_cast<CParamDisplay*> (control))
		{
			paramDisplay->setValueToStringFunction ([this] (float value, char utf8String[256], CParamDisplay* display) {
				auto string = this->value->getStringConverter ().valueAsString (value);
				auto numBytes = std::min<size_t> (string.getByteCount (), 255);
				if (numBytes)
				{
					strncpy (utf8String, string.get (), numBytes);
					utf8String[numBytes] = 0;
				}
				else
					utf8String[0] = 0;
				return true;
			});
			if (auto textEdit = dynamic_cast<CTextEdit*>(paramDisplay))
			{
				textEdit->setStringToValueFunction ([&] (UTF8StringPtr txt, float& result, CTextEdit* textEdit) {
					auto v = value->getStringConverter ().stringAsValue (txt);
					if (v == IValue::InvalidValue)
						v = value->getValue ();
					result = static_cast<float> (v);
					return true;
				});
			}
		}
		updateControlOnStateChange (control);
		control->setValueNormalized (static_cast<float> (value->getValue ()));
		control->invalid ();
		control->registerControlListener (this);
		control->registerViewListener (this);
		controls.push_back (control);
	}

	void removeControl (CControl* control)
	{
		if (auto paramDisplay = dynamic_cast<CParamDisplay*> (control))
		{
			paramDisplay->setValueToStringFunction (nullptr);
			if (auto textEdit = dynamic_cast<CTextEdit*>(paramDisplay))
				textEdit->setStringToValueFunction (nullptr);
		}
		control->unregisterViewListener (this);
		control->unregisterControlListener (this);
		auto it = std::find (controls.begin (), controls.end (), control);
		vstgui_assert (it != controls.end ());
		controls.erase (it);
	}
	
	void viewWillDelete (CView* view) override
	{
		removeControl (dynamic_cast<CControl*> (view));
	}

protected:

	using ControlList = std::vector<CControl*>;
	
	ValuePtr value;
	ControlList controls;
};

using ValueWrapperPtr = std::shared_ptr<ValueWrapper>;

//------------------------------------------------------------------------
struct WindowController::Impl : public IController, public ICommandHandler
{
	using ValueWrapperList = std::vector<ValueWrapperPtr>;
	
	Impl (WindowController& controller, const ModelBindingPtr& modelHandler, const CustomizationPtr& customization)
	: controller (controller), modelBinding (modelHandler), customization (customization)
	{
		initModelValues (modelHandler);
	}
	
	~Impl ()
	{
		if (uiDesc)
		{
			uiDesc->setSharedResources (nullptr);
			SharedResources::instance ().unuse ();
		}
	}
	
	virtual bool init (WindowPtr& inWindow, const char* fileName, const char* templateName)
	{
		window = inWindow.get ();
		if (!initUIDesc (fileName))
			return false;
		frame = owned (new CFrame ({}, nullptr));
		frame->setTransparency (true);
		this->templateName = templateName;
		
		showView ();
		
		window->setContentView (frame);
		return true;
	}
	
	virtual CPoint constraintSize (const CPoint& newSize)
	{
		CPoint p (newSize);
		if (minSize.x > 0 && minSize.y > 0)
		{
			p.x = std::max (p.x, minSize.x);
			p.y = std::max (p.y, minSize.y);
		}
		if (maxSize.x > 0 && maxSize.y > 0)
		{
			p.x = std::min (p.x, maxSize.x);
			p.y = std::min (p.y, maxSize.y);
		}
		return p;
	}
	
	virtual bool canClose () { return true; }
	
	bool initUIDesc (const char* fileName)
	{
		uiDesc = owned (new UIDescription (fileName));
		uiDesc->setSharedResources (SharedResources::instance ().get ());
		if (!uiDesc->parse ())
		{
			return false;
		}
		return true;
	}
	
	void updateMinMaxSizes ()
	{
		const UIAttributes* attr = uiDesc->getViewAttributes (templateName);
		if (!attr)
			return;
		CPoint p;
		if (attr->getPointAttribute ("minSize", p))
			minSize = p;
		else
			minSize = {};
		if (attr->getPointAttribute ("maxSize", p))
			maxSize = p;
		else
			maxSize = {};
	}
	
	void showView ()
	{
		updateMinMaxSizes ();
		auto view = uiDesc->createView (templateName, this);
		if (!view)
		{
			return;
		}
		auto viewSize = view->getViewSize ().getSize ();
		frame->getTransform ().transform (viewSize);
		frame->setSize (viewSize.x, viewSize.y);
		frame->addView (view);
		
		frame->setFocusDrawingEnabled (false);
		window->setSize (view->getViewSize ().getSize ());
	}
	
	bool canHandleCommand (const Command& command) override
	{
		if (auto commandHandler = modelBinding->dynamicCast<ICommandHandler> ())
			return commandHandler->canHandleCommand (command);
		return false;
	}
	
	bool handleCommand (const Command& command) override
	{
		if (auto commandHandler = modelBinding->dynamicCast<ICommandHandler> ())
			return commandHandler->handleCommand (command);
		return false;
	}
	
	void initModelValues (const ModelBindingPtr& modelHandler)
	{
		if (!modelHandler)
			return;
		for (auto& value : modelHandler->getValues ())
		{
			valueWrappers.emplace_back (std::make_shared<ValueWrapper> (value));
		}
	}

	// IController
	void valueChanged (CControl* control) override {}
	int32_t controlModifierClicked (CControl* control, CButtonState button) override { return 0; }
	void controlBeginEdit (CControl* control) override {}
	void controlEndEdit (CControl* control) override {}
	void controlTagWillChange (CControl* control) override
	{
		if (control->getTag () < 0)
			return;
		auto index = static_cast<ValueWrapperList::size_type>(control->getTag ());
		if (index < valueWrappers.size ())
			valueWrappers[index]->removeControl (control);
	}
	void controlTagDidChange (CControl* control) override
	{
		if (control->getTag () < 0)
			return;
		auto index = static_cast<ValueWrapperList::size_type>(control->getTag ());
		if (index < valueWrappers.size ())
			valueWrappers[index]->addControl (control);
	}
	
	int32_t getTagForName (UTF8StringPtr name, int32_t registeredTag) const override
	{
		auto it = std::find_if(valueWrappers.begin(), valueWrappers.end(), [&] (const ValueWrapperPtr& v) {
			return v->getID () == name;
		});
		if (it != valueWrappers.end())
			return static_cast<int32_t> (std::distance (valueWrappers.begin (), it));
		return registeredTag;
	}
	
	IControlListener* getControlListener (UTF8StringPtr controlTagName) override { return this; }
	CView* createView (const UIAttributes& attributes, const IUIDescription* description) override { return nullptr; }
	CView* verifyView (CView* view, const UIAttributes& attributes, const IUIDescription* description) override
	{
		auto control = dynamic_cast<CControl*> (view);
		if (control)
		{
			auto index = static_cast<ValueWrapperList::size_type>(control->getTag ());
			if (index < valueWrappers.size ())
			{
				valueWrappers[index]->updateControlOnStateChange (control);
			}
		}
		return view;
	}
	IController* createSubController (UTF8StringPtr name, const IUIDescription* description) override
	{
		if (customization)
			return customization->createController (name, this, description);
		return nullptr;
	}
	
	WindowController& controller;
	IWindow* window {nullptr};
	SharedPointer<VSTGUI::UIDescription> uiDesc;
	SharedPointer<CFrame> frame;
	UTF8String templateName;
	CPoint minSize;
	CPoint maxSize;
	ModelBindingPtr modelBinding;
	CustomizationPtr customization;
	ValueWrapperList valueWrappers;
};

#if VSTGUI_LIVE_EDITING
static const Command ToggleEditingCommand {"Debug", "Toggle Inline Editor"};

//------------------------------------------------------------------------
struct WindowController::EditImpl : WindowController::Impl
{
	EditImpl (WindowController& controller, const ModelBindingPtr& modelBinding,
	          const CustomizationPtr& customization)
	: Impl (controller, modelBinding, customization)
	{
		IApplication::instance ().registerCommand (ToggleEditingCommand, 'e');
	}
	
	bool init (WindowPtr& inWindow, const char* fileName, const char* templateName) override
	{
		this->filename = fileName;
		auto& absPath = EditFileMap::get (fileName);
		if (!absPath.empty ())
			fileName = absPath.data ();
		window = inWindow.get ();
		frame = owned (new CFrame ({}, nullptr));
		frame->setTransparency (true);

		if (!initUIDesc (fileName))
		{
			UIAttributes* attr = new UIAttributes ();
			attr->setAttribute (UIViewCreator::kAttrClass, "CViewContainer");
			attr->setAttribute ("size", "300, 300");
			uiDesc->addNewTemplate (templateName, attr);
			
			initAsNew ();
		}
		else
		{
			auto settings = uiDesc->getCustomAttributes ("UIDescFilePath", true);
			auto filePath = settings->getAttributeValue ("path");
			if (filePath)
				uiDesc->setFilePath (filePath->data ());
		}
		this->templateName = templateName;
		
		syncTags ();
		showView ();
		
		window->setContentView (frame);
		return true;
	}
	
	CPoint constraintSize (const CPoint& newSize) override
	{
		if (isEditing)
			return newSize;
		return Impl::constraintSize (newSize);
	}
	
	bool canClose () override
	{
		enableEditing (false);
		return true;
	}
	
	void initAsNew ()
	{
		if (initUIDescAsNew (*uiDesc, frame))
		{
			enableEditing (true, true);
			save (true);
			enableEditing (false);
		}
		else
		{
			Call::later ([this] () {
				window->close ();
			});
		}
	}
	
	void checkFileExists ()
	{
		auto result = checkAndUpdateUIDescFilePath (*uiDesc, frame);
		if (result == UIDescCheckFilePathResult::exists)
			return;
		if (result == UIDescCheckFilePathResult::newPathSet)
		{
			save (true);
			return;
		}
		enableEditing (false);
	}
	
	void save (bool force = false)
	{
		if (!uiEditController)
			return;
		if (force || uiEditController->getUndoManager ()->isSavePosition () == false)
		{
			if (!uiDesc->save (uiDesc->getFilePath (), uiEditController->getSaveOptions ()))
			{
				AlertBoxConfig config;
				config.headline = "Saving the uidesc file failed.";
				IApplication::instance ().showAlertBox (config);
			}
			else
				EditFileMap::set (filename, uiDesc->getFilePath ());
		}
	}

	void syncTags ()
	{
		std::list<const std::string*> tagNames;
		uiDesc->collectControlTagNames (tagNames);
		int32_t index = 0;
		for (auto& v : valueWrappers)
		{
			auto it = std::find_if (tagNames.begin(), tagNames.end(), [&] (const std::string* name) {
				return v->getID () == *name;
			});
			auto create = it == tagNames.end ();
			uiDesc->changeControlTagString (v->getID (), std::to_string (index), create);
			++index;
		}
	}
	
	void enableEditing (bool state, bool ignoreCheckFileExist = false)
	{
		if (isEditing == state && frame->getNbViews () != 0)
			return;
		isEditing = state;
		if (!state)
		{
			save ();
			uiEditController = nullptr;
		}
		
		frame->removeAll ();
		if (state)
		{
			uiDesc->setController (this);
			uiEditController = new UIEditController (uiDesc);
			auto view = uiEditController->createEditView ();
			auto viewSize = view->getViewSize ().getSize ();
			frame->getTransform ().transform (viewSize);
			frame->setSize (viewSize.x, viewSize.y);
			frame->addView (view);
			frame->enableTooltips (true);
			CColor focusColor = kBlueCColor;
			uiEditController->getEditorDescription ().getColor ("focus", focusColor);
			frame->setFocusColor (focusColor);
			frame->setFocusDrawingEnabled (true);
			frame->setFocusWidth (1);
			window->setSize (view->getViewSize ().getSize ());
			if (auto menuController = uiEditController->getMenuController ())
			{
				if (auto menu = menuController->getFileMenu ())
				{
					menu->removeAllEntry ();
					menu->setMouseEnabled (false);
				}
			}
			if (!ignoreCheckFileExist)
				checkFileExists ();
		}
		else
		{
			showView ();
		}
	}
	
	bool canHandleCommand (const Command& command) override
	{
		if (command == ToggleEditingCommand)
			return true;
		return Impl::canHandleCommand (command);
	}
	
	bool handleCommand (const Command& command) override
	{
		if (command == ToggleEditingCommand)
		{
			enableEditing (!isEditing);
			return true;
		}
		return Impl::handleCommand (command);
	}
	
	SharedPointer<UIEditController> uiEditController;
	bool isEditing {false};
	std::string filename;
};
#endif

//------------------------------------------------------------------------
bool WindowController::init (const UIDesc::Config &config, WindowPtr &window)
{
#if VSTGUI_LIVE_EDITING
	impl = std::unique_ptr<Impl> (new EditImpl (*this, config.modelBinding, config.customization));
#else
	impl = std::unique_ptr<Impl> (new Impl (*this, config.modelBinding, config.customization));
#endif
	return impl->init (window, config.uiDescFileName, config.viewName);
}

//------------------------------------------------------------------------
CPoint WindowController::constraintSize (const IWindow& window, const CPoint& newSize)
{
	return impl->constraintSize (newSize);
}

//------------------------------------------------------------------------
bool WindowController::canClose (const IWindow& window) const
{
	return impl->canClose ();
}

//------------------------------------------------------------------------
void WindowController::onClosed (const IWindow& window)
{
	impl = nullptr;
}

//------------------------------------------------------------------------
bool WindowController::canHandleCommand (const Command& command)
{
	return impl->canHandleCommand (command);
}

//------------------------------------------------------------------------
bool WindowController::handleCommand (const Command& command)
{
	return impl->handleCommand (command);
}

//------------------------------------------------------------------------
} // Anonymous

//------------------------------------------------------------------------
namespace UIDesc {

//------------------------------------------------------------------------
WindowPtr makeWindow (const Config& config)
{
	vstgui_assert (config.viewName.empty () == false);
	vstgui_assert (config.uiDescFileName.empty () == false);

	auto controller = std::make_shared<WindowController> ();

#if VSTGUI_LIVE_EDITING
	WindowConfiguration windowConfig = config.windowConfig;
	windowConfig.style.size ();
#else
	auto& windowConfig = config.windowConfig;
#endif

	auto window = IApplication::instance ().createWindow (windowConfig, controller);
	if (!window)
		return nullptr;

	if (!controller->init (config, window))
		return nullptr;

	return window;
}

//------------------------------------------------------------------------
} // UIDesc
} // Standalone
} // VSTGUI
