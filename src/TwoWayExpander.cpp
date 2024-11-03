#include "plugin.hpp"

using namespace rack;

struct expMessage { // contains both directions
	unsigned int numModulesSoFar = 0; // This will travel rightward
	unsigned int numModulesTotal = 0; // This will travel leftward
};

struct TwoWayExpander : Module {
	expMessage leftMessages[2][1]; // messages to & from left module
	expMessage rightMessages[2][1]; // messages to & from right module
	unsigned int numMe = 0;
	unsigned int numModules = 0;

	TwoWayExpander() {
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];	
		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
	} // module constructor
	
	void process(const ProcessArgs &args) override {
		bool expandsLeftward = leftExpander.module && leftExpander.module->model == modelTwoWayExpander;
		bool expandsRightward = rightExpander.module && rightExpander.module->model == modelTwoWayExpander;

		expMessage* leftSink = expandsLeftward ? (expMessage*)(leftExpander.module->rightExpander.producerMessage) : nullptr; // this is the left module's; I write to it and request flip
		expMessage* leftSource = (expMessage*)(leftExpander.consumerMessage); // this is mine; my leftExpander.producer message is written by the left module, which requests flip
		expMessage* rightSink = expandsRightward ? (expMessage*)(rightExpander.module->leftExpander.producerMessage) : nullptr; // this is the right module's; I write to it and request flip
		expMessage* rightSource = (expMessage*)(rightExpander.consumerMessage); // this is mine; my rightExpander.producer message is written by the right module, which requests flip
		
		if (expandsLeftward) { // Add to the number of modules
			numMe = leftSource->numModulesSoFar + 1;
		} else { // I'm the leftmost. Count myself.
			numMe = 1;
		}

		if (expandsRightward) {
			rightSink->numModulesSoFar = numMe; // current count
			numModules = rightSource->numModulesTotal; // total from right
			rightExpander.module->leftExpander.messageFlipRequested = true; // tell the right module to flip its leftExpander, putting the producer I wrote to into its consumer
		} else { // I'm the rightmost. Close the count loop.
			numModules = numMe;
		}

		if (expandsLeftward) {
			leftSink->numModulesTotal = numModules; // total goes left
			leftExpander.module->rightExpander.messageFlipRequested = true; // tell the left module to flip its rightExpander, putting the producer I wrote to into its consumer
		}
	} // process
}; // TwoWayExpander

inline bool calcLeftExpansion(Module* module) {
	return module && module->leftExpander.module && module->leftExpander.module->model == modelTwoWayExpander;
}
		
inline bool calcRightExpansion(Module* module) {
	return module && module->rightExpander.module && module->rightExpander.module->model == modelTwoWayExpander;
}
		
// Many thanks to MindMeld for showing how to make the widgets merge!
// Find a PanelBorder instance in the given widget's children
inline PanelBorder* findBorder(Widget* widget) {
	for (auto it = widget->children.begin(); it != widget->children.end(); ) {
		PanelBorder *bwChild = dynamic_cast<PanelBorder*>(*it);
		if (bwChild) {
			return bwChild;
		}
		else {
			++it;
		}
	}
	return NULL;
}

struct TwoWayExpanderWidget : ModuleWidget {
	PanelBorder* panelBorder;
	Label* labelHello;
	Label* labelTotal;
	Label* labelMe;
	Label* labelID;
	unsigned int oldModules = 0;
	unsigned int oldMe = 0;

	TwoWayExpanderWidget(TwoWayExpander* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/TwoWayExpander.svg")));
		SvgPanel* svgPanel = static_cast<SvgPanel*>(getPanel());
		panelBorder = findBorder(svgPanel->fb);

		// Prepare to display some text
		labelID = new Label;
		labelID->box.size = Vec(64, 5);
		labelID->box.pos = mm2px(Vec(11, 32));
		labelID->text = "";
		labelID->fontSize = 10;
		labelID->alignment = ui::Label::CENTER_ALIGNMENT;
		addChild(labelID);
		
		labelMe = new Label;
		labelMe->box.size = Vec(50, 20);
		labelMe->box.pos = mm2px(Vec(1 , 54.5));
		labelMe->text = "";
		labelMe->fontSize = 18;
		labelMe->alignment = ui::Label::CENTER_ALIGNMENT;
		addChild(labelMe);
		
		labelTotal = new Label;
		labelTotal->box.size = Vec(50, 20);
		labelTotal->box.pos = mm2px(Vec(1 , 66));
		labelTotal->text = "";
		labelTotal->fontSize = 18;
		labelTotal->alignment = ui::Label::CENTER_ALIGNMENT;
		addChild(labelTotal);
	} // widget constructor

	void draw(const DrawArgs& args) override {
		if (module && calcRightExpansion(module)) {
			DrawArgs newDrawArgs = args;
			newDrawArgs.clipBox.size.x += mm2px(0.3f); // panel has its base rectangle this much larger, to kill gap artifacts
			ModuleWidget::draw(newDrawArgs);
		} else {
			ModuleWidget::draw(args);
		}
	} // draw
	
	void step() override {
		if (module) {
			TwoWayExpander* module = static_cast<TwoWayExpander*>(this->module);
			
			// Hide borders to show expansion
			int leftShift = (calcLeftExpansion(module) ? 3 : 0);
			int rightEnlarge = (calcRightExpansion(module) ? 4 : 0);
			if (panelBorder->box.size.x != (box.size.x + leftShift + rightEnlarge)) {
				panelBorder->box.pos.x = -leftShift; // move it over so left side won't display
				panelBorder->box.size.x = (box.size.x + leftShift + rightEnlarge); // make it bigger so right side won't display
				SvgPanel* svgPanel = static_cast<SvgPanel*>(getPanel());
				svgPanel->fb->dirty = true;
			}
			// Update labels if required
			if (labelID->text == "") labelID->text = string::f("%016" PRId64,module->id);
			if (oldMe != module->numMe) {
				labelMe->text = string::f("%u",module->numMe);
				oldMe = module->numMe;
			}
			if (oldModules != module->numModules) {
				labelTotal->text = string::f("%u",module->numModules);
				oldModules = module->numModules;
			}
		}
		ModuleWidget::step();
	} // step
}; // TwoWayExpanderWidget

Model* modelTwoWayExpander = createModel<TwoWayExpander, TwoWayExpanderWidget>("TwoWayExpander");
