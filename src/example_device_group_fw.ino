 /**
 ******************************************************************************
  Copyright (c) 2013-2020 Particle Industries, Inc.  All rights reserved.
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

SerialLogHandler logHandler(LOG_LEVEL_WARN, {
    {"app", LOG_LEVEL_ALL}
});

// Particle protocol sets a maximum event name size - must enforce in code
#define MAX_EVENT_NAME_LENGTH particle::protocol::MAX_EVENT_NAME_LENGTH

// --------------------------------------------------
// EEPROM-based device configuration
// --------------------------------------------------

// DeviceConfig_t parameters
#define EEPROM_VERSION      0x01
#define MAX_DEVICE_GROUPS   16
#define MAX_USER_ID_LEN     8

struct DeviceConfig_t {
    uint8_t version;
    char    userID[MAX_USER_ID_LEN+1];          // Add one char for termination char
    uint8_t numGroups;
    uint8_t deviceGroups[MAX_DEVICE_GROUPS];
};

DeviceConfig_t config;

void writeConfigToEEPROM() {
    EEPROM.put(0, config);
    Log.info("New config written to EEPROM");
    printConfig();
}

void printConfig() {
    Log.trace("EEPROM Configuration:");
    Log.trace("    Config Version:    %02X", config.version);
    Log.trace("    User ID:           %s", config.userID);
    Log.trace("    Num Device Groups: %u", config.numGroups);
    Log.trace("    Device Groups:     %s", uint8ArrToString(config.deviceGroups, MAX_DEVICE_GROUPS).c_str());
}

// --------------------------------------------------
// Main code
// --------------------------------------------------

void setup() {

    // Functions and variables should always be set up first
    Particle.function("addToGroup", addToGroup);
    Particle.function("clearGroups", clearGroups);
    Particle.function("setUserID", setUserID);
    
    // Optional wait for serial connection
    //while (!Serial.isConnected()) {Particle.process();}
    //delay(1000);

    // Attempt to configure device from EEPROM
    Log.info("Attempt to configure from EEPROM...");
    Log.trace("EEPROM length: %u bytes", (unsigned int)EEPROM.length());
    Log.trace("Config length: %u bytes", (unsigned int)sizeof(config));
    EEPROM.get(0, config);
    Log.trace("EEPROM read");

    if (config.version == 0xFF) {
        // Device is unconfigured - set some sane defaults
        Log.info("EEPROM unconfigured: setting defaults");
        config.version = EEPROM_VERSION;
        memset(config.userID, 0, (size_t)(MAX_USER_ID_LEN+1));
        config.numGroups = 0;
        for (int i = 0; i < MAX_DEVICE_GROUPS; i++) {
            config.deviceGroups[i] = 0;
        }
    } else {
        Log.info("Configure success");
    }

    // Share our current configuration
    printConfig();

    // Set up our subscription to events to the configured User ID
    Particle.subscribe(config.userID, parseMessage, MY_DEVICES);
}

void loop() {
    // Nothing to see here
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------
// Parse event callback function
// Example events:
// "123ABC/set_req_output"				                Runs set_requested_output function of all devices belonging to user 123ABC (agnostic of groups)
// "123ABC/set_req_output/group1"		                Runs set_requested_output function of all devices belonging to user 123ABC in group "group1"
// "123ABC/set_req_output/e00fce68f5048fcadf1ea38a"		Runs set_requested_output function on device e00fce68f5048fcadf1ea38a, if it belongs to user 123ABC
// ---------------------------------------------------------------------------------------------------------------------------------------------------------
void parseMessage(const char *event, const char *data) {
    Log.trace("Event received {\n\tevent: %s\n\tdata: %s\n}", event, data);
	
    // Local copy of the event string.  strtok modifies its input string.
	char event_name[MAX_EVENT_NAME_LENGTH];
	strncpy(event_name, event, MAX_EVENT_NAME_LENGTH);
	event_name[MAX_EVENT_NAME_LENGTH-1] = 0x0;	// strncpy does not guarantee the destination is a valid c string if the source is longer than the number of bytes.  Enforce proper termination

    // --------------------------------------------------
	// tokenize on slashes to go down in hierarchy:
	// <userID>/<command>/<group or Device ID, optional>
    // --------------------------------------------------

	// Parse the <userID> level of the hierarchy
	// Note: this should always return true, since the Particle.subscribe function is subscribing to all events that start with this!
	char* parsedUserID = strtok(event_name, "/");
	if (parsedUserID != NULL && strcmp(parsedUserID, config.userID) == 0) {
        Log.trace("Event parsed %s", parsedUserID);
		
		// Parse the <command> level of the event hierarchy
		char* parsedFunction = strtok(NULL, "/");
		if (parsedFunction != NULL) {
            Log.trace("Command parsed: %s", parsedFunction);

			// Parse the optional <group> level of the event hierarchy
			char* parsedGroup = strtok(NULL, "/");
            bool groupMatched = false;
            
            if (parsedGroup == NULL) {
                // No group supplied means all groups are addressed
                groupMatched = true;
                Log.info("All groups addressed!");
            } else if (strcmp(parsedGroup, System.deviceID().c_str()) == 0) {
                // Our Device ID supplied as a group means we were addressed directly
                groupMatched = true;
                Log.info("Device addressed by Device ID");                
            } else {
                // Anything else received - check if it matches a group ID we belong to
                uint32_t parsedDeviceGroup = (uint32_t)strtol(parsedGroup, NULL, 10);
                for (int i = 0; i < config.numGroups; i++) {
                    if (parsedDeviceGroup == config.deviceGroups[i]) {
                        groupMatched = true;
                        break;
                    }
                }
            }

            if (groupMatched) {
                // Matched a group we belong to - run the required function
                Log.info("Group parsed: %s", parsedGroup);

				if      (!strcmp(parsedFunction, "f1"))  function1(data);   // Call "function1"
                else if (!strcmp(parsedFunction, "f2"))  function2(data);   // Call "function2"
				else if (!strcmp(parsedFunction, "f3"))  function3(data);   // Call "function3"
                else Log.warn("Unknown function received: %s", parsedFunction);

			} else {
                // A group was addressed, but it wasn't ours - ignore the received event
                Log.info("Device's group was NOT addressed: %s", parsedGroup);
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------------
// These functions are called from events sent to the device
// Replace these with your functions, and their references above for naming
// ----------------------------------------------------------------------------------------------------

// TODO: make these actually do something physical (change the LED color for example, or read a sensor)
void function1(const char* data) {
    Log.info("function1: %s", data);
}

void function2(const char* data) {
    Log.info("function2: %s", data);
}

void function3(const char* data) {
    Log.info("function3: %s", data);
}

// Helper function to convert uint8_t array to a String in the format "{1, 2, 3, 4}"
String uint8ArrToString(uint8_t* arr, size_t arr_len) {
    String s = "{";
    for (size_t i = 0; i < arr_len; i++) {
        s += arr[i];
        if (i < arr_len - 1) s += ", "; // Separator...
        else                 s += "}";  // ...or terminator
    }
    return s;
}

// ----------------------------------------------------------------------------------------------------
// The following functions are exposed as Particle.function() instances in the cloud
// ----------------------------------------------------------------------------------------------------

int addToGroup(String extra) {
    // Parse function argument and do some basic range checking, as groups are specified as a uint8_t
    int parsedGroup = (int)strtol(extra.c_str(), NULL, 10);
    if (parsedGroup > 0 && parsedGroup < 256) {
        if (config.numGroups < MAX_DEVICE_GROUPS) {
            config.deviceGroups[config.numGroups] = (uint8_t)parsedGroup;
            config.numGroups++;
            writeConfigToEEPROM();
            Log.info("Device added to group %u (%u groups total)", (uint8_t)parsedGroup, config.numGroups);
            return parsedGroup;
        } else {
            int retcode = -2;
            Log.code(retcode).details("Max number of groups exceeded").warn("Cannot add device to group %u", (uint8_t)parsedGroup);
            return retcode;  // No more room to add groups
        }
    } else {
        int retcode = -1;
        Log.code(retcode).details("Group number invalid").warn("Invalid group received: %s", extra.c_str());
        return retcode;  // Out of range
    }
}

int clearGroups(String extra) {
    config.numGroups = 0;
    writeConfigToEEPROM();
    Log.info("Groups cleared");
    return 0;
}

int setUserID(String extra) {
    // Basic bounds checking for sanity
    if (extra.length() > 0) {
        // Process our new User ID (restrict length to MAX_USER_ID_LEN) and write configuration
        strncpy(config.userID, extra.c_str(), MAX_USER_ID_LEN);
        config.userID[MAX_USER_ID_LEN] = 0x0;    // Ensure termination char for safety
        writeConfigToEEPROM();

        // Resubscribe to events addressed at our new User
        Particle.unsubscribe();
        Particle.subscribe(config.userID, parseMessage, MY_DEVICES);
        
        Log.info("New user ID set: %s", config.userID);
    } else {
        int retcode = -1;
        Log.code(retcode).details("Empty string received").warn("Reject new User ID setting");
        return retcode;
    }
    return 0;
}