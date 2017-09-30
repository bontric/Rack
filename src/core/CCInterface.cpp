#include <assert.h>
#include <list>
#include <algorithm>
#include <portmidi.h>
#include "core.hpp"
#include "gui.hpp"
#include <string>

using namespace rack;

const int NUM_OUT = 16;

struct CCInterface : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
	  NUM_OUTPUTS=NUM_OUT
	};

	int portId = -1;
	PortMidiStream *stream = NULL;
	std::list<int> notes;
	/** Filter MIDI channel
	-1 means all MIDI channels
	*/
	int channel = -1;
	int cc[NUM_OUTPUTS];
	int firstCC = 0;

	bool retrigger = false;
	bool retriggered = false;

	CCInterface();
	~CCInterface();
	void step();

	int getPortCount();
	std::string getPortName(int portId);
	// -1 will close the port
	void setPortId(int portId);
	void setChannel(int channel) {
		this->channel = channel;
	}
	void setFirstCC(int firstCC) {
		this->firstCC = firstCC;
	}
	void pressNote(int note);
	void releaseNote(int note);
	void processMidi(long msg);

	json_t *toJson() {
		json_t *rootJ = json_object();
		if (portId >= 0) {
			std::string portName = getPortName(portId);
			json_object_set_new(rootJ, "portName", json_string(portName.c_str()));
			json_object_set_new(rootJ, "channel", json_integer(channel));
			json_object_set_new(rootJ, "firstCC", json_integer(firstCC));
		}
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *portNameJ = json_object_get(rootJ, "portName");
		if (portNameJ) {
			std::string portName = json_string_value(portNameJ);
			for (int i = 0; i < getPortCount(); i++) {
				if (portName == getPortName(i)) {
					setPortId(i);
					break;
				}
			}
		}

		json_t *channelJ = json_object_get(rootJ, "channel");
		if (channelJ) {
			setChannel(json_integer_value(channelJ));
		}

		json_t *firstCCJ = json_object_get(rootJ, "firstCC");
		if (firstCCJ) {
			setFirstCC(json_integer_value(firstCCJ));
		}
	}

	void initialize() {
		setPortId(-1);
		for (int i = 0 ; i < NUM_OUTPUTS ; i++ ){
			  cc[i] = 0;
		}
	}
};


CCInterface::CCInterface() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);
	midiInit();
}

CCInterface::~CCInterface() {
	setPortId(-1);
}

void CCInterface::step() {
	if (stream) {
		// Read MIDI events
		PmEvent event;
		while (Pm_Read(stream, &event, 1) > 0) {
			processMidi(event.message);
		}
	}

	for (int i = 0 ; i < NUM_OUTPUTS ; i++){
	  if (outputs[i]) {
		*outputs[i] = cc[i] / 127.0 * 10.0;
	}

	}

}

int CCInterface::getPortCount() {
	return Pm_CountDevices();
}

std::string CCInterface::getPortName(int portId) {
	const PmDeviceInfo *info = Pm_GetDeviceInfo(portId);
	if (!info)
		return "";
	return stringf("%s: %s (%s)", info->interf, info->name, info->input ? "input" : "output");
}

void CCInterface::setPortId(int portId) {
	PmError err;

	// Close existing port
	if (stream) {
		err = Pm_Close(stream);
		if (err) {
			printf("Failed to close MIDI port: %s\n", Pm_GetErrorText(err));
		}
		stream = NULL;
	}
	this->portId = -1;

	// Open new port
	if (portId >= 0) {
		err = Pm_OpenInput(&stream, portId, NULL, 128, NULL, NULL);
		if (err) {
			printf("Failed to open MIDI port: %s\n", Pm_GetErrorText(err));
			return;
		}
	}
	this->portId = portId;
}

void CCInterface::processMidi(long msg) {
	int channel = msg & 0xf;
	int status = (msg >> 4) & 0xf;
	int data1 = (msg >> 8) & 0xff;
	int data2 = (msg >> 16) & 0xff;
	// printf("channel %d status %d data1 %d data2 %d\n", channel, status, data1, data2);

	// Filter channels
	if (this->channel >= 0 && this->channel != channel)
		return;


	if (status == 0xb && data1 >= this->firstCC && data1 < this->firstCC + NUM_OUTPUTS)
	  this->cc[data1-this->firstCC] = data2;


}


struct MidiItem : MenuItem {
	CCInterface *midiInterface;
	int portId;
	void onAction() {
		midiInterface->setPortId(portId);
	}
};

struct MidiChoice : ChoiceButton {
	CCInterface *midiInterface;
	void onAction() {
		Menu *menu = gScene->createMenu();
		menu->box.pos = getAbsolutePos().plus(Vec(0, box.size.y));
		menu->box.size.x = box.size.x;

		int portCount = midiInterface->getPortCount();
		{
			MidiItem *midiItem = new MidiItem();
			midiItem->midiInterface = midiInterface;
			midiItem->portId = -1;
			midiItem->text = "No device";
			menu->pushChild(midiItem);
		}
		for (int portId = 0; portId < portCount; portId++) {
			MidiItem *midiItem = new MidiItem();
			midiItem->midiInterface = midiInterface;
			midiItem->portId = portId;
			midiItem->text = midiInterface->getPortName(portId);
			menu->pushChild(midiItem);
		}
	}
	void step() {
		std::string name = midiInterface->getPortName(midiInterface->portId);
		text = ellipsize(name, 8);
	}
};

struct ChannelItem : MenuItem {
	CCInterface *midiInterface;
	int channel;
	void onAction() {
		midiInterface->setChannel(channel);
	}
};

struct ChannelChoice : ChoiceButton {
	CCInterface *midiInterface;
	void onAction() {
		Menu *menu = gScene->createMenu();
		menu->box.pos = getAbsolutePos().plus(Vec(0, box.size.y));
		menu->box.size.x = box.size.x;

		{
			ChannelItem *channelItem = new ChannelItem();
			channelItem->midiInterface = midiInterface;
			channelItem->channel = -1;
			channelItem->text = "All";
			menu->pushChild(channelItem);
		}
		for (int channel = 0; channel < 16; channel++) {
			ChannelItem *channelItem = new ChannelItem();
			channelItem->midiInterface = midiInterface;
			channelItem->channel = channel;
			channelItem->text = stringf("%d", channel + 1);
			menu->pushChild(channelItem);
		}
	}
	void step() {
		text = (midiInterface->channel >= 0) ? stringf("%d", midiInterface->channel + 1) : "All";
	}
};

struct FirstCCItem : MenuItem {
	CCInterface *midiInterface;
	int firstCC;
	void onAction() {
		midiInterface->setFirstCC(firstCC);
	}
};

struct FirstCCChoice : ChoiceButton {
	CCInterface *midiInterface;
	void onAction() {
		Menu *menu = gScene->createMenu();
		menu->box.pos = getAbsolutePos().plus(Vec(0, box.size.y));
		menu->box.size.x = box.size.x;

		for (int firstCC = 0; firstCC < 120; firstCC+=10) {
			FirstCCItem *firstCCItem = new FirstCCItem();
			firstCCItem->midiInterface = midiInterface;
			firstCCItem->firstCC = firstCC;
			firstCCItem->text = stringf("%d", firstCC);
			menu->pushChild(firstCCItem);
		}
	}
	void step() {
		text = stringf("%d", midiInterface->firstCC);
	}
};


CCInterfaceWidget::CCInterfaceWidget() {
	CCInterface *module = new CCInterface();
	setModule(module);
	box.size = Vec(15*19, 380);

	{
		Panel *panel = new LightPanel();
		panel->box.size = box.size;
		addChild(panel);
	}

	float margin = 5;
	float labelHeight = 15;
	float yPos = margin;

	{
		Label *label = new Label();
		label->box.pos = Vec(margin, yPos);
		label->text = "MIDI CC device";
		addChild(label);
		yPos += labelHeight + margin;

		MidiChoice *midiChoice = new MidiChoice();
		midiChoice->midiInterface = dynamic_cast<CCInterface*>(module);
		midiChoice->box.pos = Vec(margin, yPos);
		midiChoice->box.size.x = box.size.x - 10;
		addChild(midiChoice);
		yPos += midiChoice->box.size.y + margin;
	}

	{
		Label *label = new Label();
		label->box.pos = Vec(margin, yPos);
		label->text = "Channel";
		addChild(label);
		yPos += labelHeight + margin;

		ChannelChoice *channelChoice = new ChannelChoice();
		channelChoice->midiInterface = dynamic_cast<CCInterface*>(module);
		channelChoice->box.pos = Vec(margin, yPos);
		channelChoice->box.size.x = box.size.x - 10;
		addChild(channelChoice);
		yPos += channelChoice->box.size.y + margin;
	}

	{
		Label *label = new Label();
		label->box.pos = Vec(margin, yPos);
		label->text = "FirstCC";
		addChild(label);
		yPos += labelHeight + margin;

		FirstCCChoice *firstCCChoice = new FirstCCChoice();
		firstCCChoice->midiInterface = dynamic_cast<CCInterface*>(module);
		firstCCChoice->box.pos = Vec(margin, yPos);
		firstCCChoice->box.size.x = box.size.x - 10;
		addChild(firstCCChoice);
		yPos += firstCCChoice->box.size.y + margin;
	}


	for (int i = 0 ; i < CCInterface::NUM_OUTPUTS ; i ++) {
		Label *label = new Label();
		label->box.pos = Vec(margin+(i%4)*(margin+62), yPos);
		label->text = std::to_string(i);
		addChild(label);
		yPos += labelHeight + margin;

		addOutput(createOutput<PJ3410Port>(Vec(28 + (i%4)*(margin+62), yPos), module, i));

		if ((i+1)%4 == 0) {
			yPos += 30 + margin;
		}else {
			yPos -= labelHeight + margin;
		}
	}

}

void CCInterfaceWidget::step() {
	// Assume QWERTY
#define MIDI_KEY(key, midi) if (glfwGetKey(gWindow, key)) printf("%d\n", midi);

	// MIDI_KEY(GLFW_KEY_Z, 48);

	ModuleWidget::step();
}
