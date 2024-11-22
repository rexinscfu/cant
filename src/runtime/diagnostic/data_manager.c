#include "data_manager.h"
#include <string.h>
#include "../os/critical.h"
#include "session_manager.h"

#define MAX_IDENTIFIERS 200

typedef struct {
    DataManagerConfig config;
    DataIdentifier identifiers[MAX_IDENTIFIERS];
    uint32_t identifier_count;
    bool initialized;
    CriticalSection critical;
} DataManager;

static DataManager data_manager;

static DataIdentifier* find_identifier(uint16_t did) {
    for (uint32_t i = 0; i < data_manager.identifier_count; i++) {
        if (data_manager.identifiers[i].did == did) {
            return &data_manager.identifiers[i];
        }
    }
    return NULL;
}

static bool apply_scaling(const DataIdentifier* identifier, uint8_t* data, uint16_t length, bool to_raw) {
    switch (identifier->scaling) {
        case SCALING_NONE:
            return true;

        case SCALING_LINEAR: {
            // Example linear scaling implementation
            if (identifier->type == DATA_TYPE_FLOAT) {
                float* value = (float*)data;
                if (to_raw) {
                    *value = (*value * 100.0f); // Example scale factor
                } else {
                    *value = (*value / 100.0f);
                }
            }
            return true;
        }

        case SCALING_INVERSE: {
            // Example inverse scaling implementation
            if (identifier->type == DATA_TYPE_FLOAT) {
                float* value = (float*)data;
                if (*value != 0) {
                    *value = 1.0f / *value;
                }
            }
            return true;
        }

        case SCALING_FORMULA:
            // Custom formula scaling would be implemented here
            return true;

        case SCALING_TABLE:
            // Table lookup scaling would be implemented here
            return true;

        default:
            return false;
    }
}

bool Data_Manager_Init(const DataManagerConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&data_manager.critical);

    memcpy(&data_manager.config, config, sizeof(DataManagerConfig));
    
    // Copy initial identifiers if provided
    if (config->identifiers && config->identifier_count > 0) {
        uint32_t copy_count = (config->identifier_count <= MAX_IDENTIFIERS) ? 
                             config->identifier_count : MAX_IDENTIFIERS;
        memcpy(data_manager.identifiers, config->identifiers, 
               sizeof(DataIdentifier) * copy_count);
        data_manager.identifier_count = copy_count;
    } else {
        data_manager.identifier_count = 0;
    }

    init_critical(&data_manager.critical);
    data_manager.initialized = true;

    exit_critical(&data_manager.critical);
    return true;
}

void Data_Manager_DeInit(void) {
    enter_critical(&data_manager.critical);
    memset(&data_manager, 0, sizeof(DataManager));
    exit_critical(&data_manager.critical);
}

bool Data_Manager_ReadData(uint16_t did, uint8_t* data, uint16_t* length) {
    if (!data_manager.initialized || !data || !length) {
        return false;
    }

    enter_critical(&data_manager.critical);

    DataIdentifier* identifier = find_identifier(did);
    if (!identifier) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Check access rights
    if (!(identifier->access_rights & DATA_ACCESS_READ)) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Check security level
    SessionState session_state = Session_Manager_GetState();
    if (session_state.security_level < identifier->security_level) {
        exit_critical(&data_manager.critical);
        return false;
    }

    bool result = false;
    if (identifier->read_handler) {
        // Use custom read handler
        result = identifier->read_handler(did, data, length);
    } else if (identifier->data_ptr) {
        // Direct memory access
        if (*length >= identifier->length) {
            memcpy(data, identifier->data_ptr, identifier->length);
            *length = identifier->length;
            result = true;
        }
    }

    if (result) {
        // Apply scaling if needed
        result = apply_scaling(identifier, data, *length, false);
        
        // Notify access if callback is configured
        if (data_manager.config.access_callback) {
            data_manager.config.access_callback(did, DATA_ACCESS_READ, true);
        }
    }

    exit_critical(&data_manager.critical);
    return result;
}

bool Data_Manager_WriteData(uint16_t did, const uint8_t* data, uint16_t length) {
    if (!data_manager.initialized || !data) {
        return false;
    }

    enter_critical(&data_manager.critical);

    DataIdentifier* identifier = find_identifier(did);
    if (!identifier) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Check access rights
    if (!(identifier->access_rights & DATA_ACCESS_WRITE)) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Check security level
    SessionState session_state = Session_Manager_GetState();
    if (session_state.security_level < identifier->security_level) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Create temporary buffer for scaling
    uint8_t scaled_data[256];
    if (length > sizeof(scaled_data)) {
        exit_critical(&data_manager.critical);
        return false;
    }
    memcpy(scaled_data, data, length);

    // Apply scaling if needed
    if (!apply_scaling(identifier, scaled_data, length, true)) {
        exit_critical(&data_manager.critical);
        return false;
    }

    bool result = false;
    if (identifier->write_handler) {
        // Use custom write handler
        result = identifier->write_handler(did, scaled_data, length);
    } else if (identifier->data_ptr && length == identifier->length) {
        // Direct memory access
        memcpy(identifier->data_ptr, scaled_data, length);
        result = true;
    }

    if (result && data_manager.config.access_callback) {
        data_manager.config.access_callback(did, DATA_ACCESS_WRITE, true);
    }

    exit_critical(&data_manager.critical);
    return result;
}

bool Data_Manager_AddIdentifier(const DataIdentifier* identifier) {
    if (!data_manager.initialized || !identifier) {
        return false;
    }

    enter_critical(&data_manager.critical);

    // Check if identifier already exists
    if (find_identifier(identifier->did)) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Check if we have space for new identifier
    if (data_manager.identifier_count >= MAX_IDENTIFIERS) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Add new identifier
    memcpy(&data_manager.identifiers[data_manager.identifier_count], 
           identifier, sizeof(DataIdentifier));
    data_manager.identifier_count++;

    exit_critical(&data_manager.critical);
    return true;
}

bool Data_Manager_RemoveIdentifier(uint16_t did) {
    if (!data_manager.initialized) {
        return false;
    }

    enter_critical(&data_manager.critical);

    // Find identifier index
    int32_t index = -1;
    for (uint32_t i = 0; i < data_manager.identifier_count; i++) {
        if (data_manager.identifiers[i].did == did) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        exit_critical(&data_manager.critical);
        return false;
    }

    // Remove identifier by shifting remaining ones
    if (index < (data_manager.identifier_count - 1)) {
        memmove(&data_manager.identifiers[index],
                &data_manager.identifiers[index + 1],
                sizeof(DataIdentifier) * (data_manager.identifier_count - index - 1));
    }

    data_manager.identifier_count--;

    exit_critical(&data_manager.critical);
    return true;
}

DataIdentifier* Data_Manager_GetIdentifier(uint16_t did) {
    if (!data_manager.initialized) {
        return NULL;
    }

    enter_critical(&data_manager.critical);
    DataIdentifier* identifier = find_identifier(did);
    exit_critical(&data_manager.critical);

    return identifier;
}

bool Data_Manager_HasAccess(uint16_t did, DataAccessRight access) {
    if (!data_manager.initialized) {
        return false;
    }

    enter_critical(&data_manager.critical);

    DataIdentifier* identifier = find_identifier(did);
    if (!identifier) {
        exit_critical(&data_manager.critical);
        return false;
    }

    SessionState session_state = Session_Manager_GetState();
    bool has_access = (identifier->access_rights & access) && 
                     (session_state.security_level >= identifier->security_level);

    exit_critical(&data_manager.critical);
    return has_access;
}

uint32_t Data_Manager_GetIdentifierCount(void) {
    if (!data_manager.initialized) {
        return 0;
    }
    return data_manager.identifier_count;
}