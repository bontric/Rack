#include "rack.hpp"


using namespace rack;

////////////////////
// module widgets
////////////////////

struct AudioInterfaceWidget : ModuleWidget {
	AudioInterfaceWidget();
};

struct MidiInterfaceWidget : ModuleWidget {
	MidiInterfaceWidget();
	void step();
};

struct CCInterfaceWidget : ModuleWidget {
	CCInterfaceWidget();
	void step();
};

static bool midi_initialized = false;
void midiInit();
