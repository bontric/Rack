#include "core.hpp"


struct CorePlugin : Plugin {
	CorePlugin() {
		slug = "Core";
		name = "Core";
		createModel<AudioInterfaceWidget>(this, "AudioInterface", "Audio Interface");
		createModel<MidiInterfaceWidget>(this, "MidiInterface", "MIDI Interface");
		createModel<CCInterfaceWidget>(this, "CCInterface", "CC Interface");
	}
};


Plugin *init() {
	return new CorePlugin();
}
