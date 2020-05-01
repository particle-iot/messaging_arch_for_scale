/* Title:  Example code for the "Messaging Architecture for Scale" White Paper
 * Date:   April, 2020
 * Author: Dan Kouba, Solutions Architect
 * Email:  dan.kouba@particle.io
 */

SerialLogHandler logHandler(LOG_LEVEL_WARN, {
    {"app", LOG_LEVEL_ALL}
});

// Particle protocol sets a maximum event name size
constexpr auto MAX_EVENT_NAME_LENGTH = particle::protocol::MAX_EVENT_NAME_LENGTH;

// Limits for the lengths of some variables
constexpr auto MAX_USER_ID_LEN = 8;
constexpr auto MAX_DEVICE_GROUPS = 16;

// --------------------------------------------------
// EEPROM-based device configuration
// --------------------------------------------------

// DeviceConfig_t parameters
constexpr auto EEPROM_VERSION = 0x01;

struct DeviceConfig_t {
    uint8_t version;
    char    userID[MAX_USER_ID_LEN+1];          // Add one char for termination char
    uint8_t numGroups;
    uint8_t deviceGroups[MAX_DEVICE_GROUPS];
};

DeviceConfig_t config;

void printConfig(DeviceConfig_t &config) {
    Log.trace("EEPROM Configuration:");
    Log.trace("    Config Version:    %02X", config.version);
    Log.trace("    User ID:           \"%s\"", config.userID);
    Log.trace("    Num Device Groups: %u", config.numGroups);
    Log.trace("    Device Groups:     %s", uint8ArrToString(config.deviceGroups, MAX_DEVICE_GROUPS).c_str());
}

void writeConfigToEEPROM(DeviceConfig_t &config) {
    EEPROM.put(0, config);
    Log.info("New config written to EEPROM");
    printConfig(config);
}

void readOrInitEEPROM(DeviceConfig_t &config) {
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
        strcpy(config.userID, "000000");
        config.numGroups = 0;
        for (int i = 0; i < MAX_DEVICE_GROUPS; i++) {
            config.deviceGroups[i] = 0;
        }
    } else {
        Log.info("Configure success");
    }

    // Show what we have done
    printConfig(config);
}

// --------------------------------------------------
// Main code
// --------------------------------------------------

void setup() {

    // Functions and variables should always be set up first
    Particle.function("addToGroup", addToGroup);
    Particle.function("clearGroups", clearGroups);
    Particle.function("setUserID", setUserID);

    // Using the RGB LED as an "output" for this example
    RGB.control(true);

    // Read our EEPROM configuration
    readOrInitEEPROM(config);

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
    char event_data[strlen(data)+1];
    strcpy(event_data, data);
    // --------------------------------------------------
    // tokenize on slashes to go down in hierarchy:
    // <userID>/<command>/<group or Device ID, optional>
    // --------------------------------------------------

    // Parse the <userID> level of the hierarchy
    // Note: this should always return true, since the Particle.subscribe function is subscribing to all events that start with this!
    char* parsedUserID = strtok(event_data, "/");
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
                for (int i = 0; i < MAX_DEVICE_GROUPS; i++) {
                    if (parsedDeviceGroup == config.deviceGroups[i]) {
                        groupMatched = true;
                        break;
                    }
                }
            }

            if (groupMatched) {
                // Matched a group we belong to - run the required function
                Log.info("Group parsed: %s", parsedGroup);

                if      (!strcmp(parsedFunction, "red"))   red(data);
                else if (!strcmp(parsedFunction, "green")) green(data);
                else if (!strcmp(parsedFunction, "blue"))  blue(data);
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
void red(const char* data) {
    Log.info("red() called with data: %s", data);
    RGB.color(255, 0, 0);
}

void green(const char* data) {
    Log.info("green() called with data: %s", data);
    RGB.color(0, 255, 0);
}

void blue(const char* data) {
    Log.info("blue() called with data: %s", data);
    RGB.color(0, 0, 255);
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
            writeConfigToEEPROM(config);
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
    writeConfigToEEPROM(config);
    Log.info("Groups cleared");
    return 0;
}

int setUserID(String extra) {
    // Basic bounds checking for sanity
    if (extra.length() > 0) {
        // Process our new User ID (restrict length to MAX_USER_ID_LEN) and write configuration
        strncpy(config.userID, extra.c_str(), MAX_USER_ID_LEN);
        config.userID[MAX_USER_ID_LEN] = 0x0;    // Ensure termination char for safety
        writeConfigToEEPROM(config);

        // Resubscribe to events addressed at our new User
        Particle.unsubscribe(); // NOTE: THIS WILL UNSUBSCRIBE FROM ALL SUBSCRIPTIONS
        Particle.subscribe(config.userID, parseMessage, MY_DEVICES);
        
        Log.info("New user ID set: %s", config.userID);
    } else {
        int retcode = -1;
        Log.code(retcode).details("Empty string received").warn("Reject new User ID setting");
        return retcode;
    }
    return 0;
}