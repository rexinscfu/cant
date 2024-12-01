#include "unity.h"
#include "../../src/runtime/diagnostic/diag_recorder.h"
#include "../../src/runtime/diagnostic/diag_timer.h"
#include <string.h>

// Test context
typedef struct {
    uint32_t custom_record_count;
} TestContext;

static TestContext test_ctx;

// Mock time functions
static uint32_t mock_time = 0;
uint32_t mock_get_timestamp(void) {
    return mock_time;
}
void mock_advance_time(uint32_t ms) {
    mock_time += ms;
}

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    mock_time = 0;
    
    DiagTimer_SetTimestampFunction(mock_get_timestamp);
    DiagTimer_Init();
    
    DiagRecorderConfig config = {
        .max_entries = 100,
        .circular_buffer = true,
        .auto_start = true,
        .export_path = "recorder_export.txt"
    };
    DiagRecorder_Init(&config);
}

void tearDown(void) {
    DiagRecorder_Deinit();
    DiagTimer_Deinit();
}

// Basic recording tests
void test_DiagRecorder_BasicRecording(void) {
    DiagRecordEntry entry = {
        .timestamp = mock_get_timestamp(),
        .type = RECORD_TYPE_MESSAGE,
        .sequence = 1,
        .data.message = {
            .service_id = 0x10,
            .sub_function = 0x01,
            .data_length = 3,
            .data = {0x01, 0x02, 0x03}
        }
    };
    
    add_entry(&entry);
    
    TEST_ASSERT_EQUAL(1, DiagRecorder_GetEntryCount());
    
    const DiagRecordEntry* retrieved = DiagRecorder_GetEntry(0);
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL(entry.timestamp, retrieved->timestamp);
    TEST_ASSERT_EQUAL(entry.type, retrieved->type);
    TEST_ASSERT_EQUAL(entry.sequence, retrieved->sequence);
    TEST_ASSERT_EQUAL_MEMORY(&entry.data.message, &retrieved->data.message, 
                             sizeof(entry.data.message));
}

void test_DiagRecorder_CircularBuffer(void) {
    // Fill buffer
    for (uint32_t i = 0; i < 100; i++) {
        DiagRecordEntry entry = {
            .timestamp = mock_get_timestamp(),
            .type = RECORD_TYPE_MESSAGE,
            .sequence = i,
            .data.message = {
                .service_id = 0x10,
                .sub_function = 0x01,
                .data_length = 1,
                .data = {i}
            }
        };
        add_entry(&entry);
    }
    
    TEST_ASSERT_EQUAL(100, DiagRecorder_GetEntryCount());
    
    // Add one more to trigger circular behavior
    DiagRecordEntry new_entry = {
        .timestamp = mock_get_timestamp(),
        .type = RECORD_TYPE_MESSAGE,
        .sequence = 100,
        .data.message = {
            .service_id = 0x10,
            .sub_function = 0x01,
            .data_length = 1,
            .data = {0xFF}
        }
    };
    add_entry(&new_entry);
    
    TEST_ASSERT_EQUAL(100, DiagRecorder_GetEntryCount());
    
    const DiagRecordEntry* first_entry = DiagRecorder_GetEntry(0);
    TEST_ASSERT_NOT_NULL(first_entry);
    TEST_ASSERT_EQUAL(new_entry.sequence, first_entry->sequence);
    TEST_ASSERT_EQUAL_MEMORY(&new_entry.data.message, &first_entry->data.message, 
                             sizeof(new_entry.data.message));
}

void test_DiagRecorder_ExportToFile(void) {
    // Add some entries
    for (uint32_t i = 0; i < 10; i++) {
        DiagRecordEntry entry = {
            .timestamp = mock_get_timestamp(),
            .type = RECORD_TYPE_MESSAGE,
            .sequence = i,
            .data.message = {
                .service_id = 0x10,
                .sub_function = 0x01,
                .data_length = 1,
                .data = {i}
            }
        };
        add_entry(&entry);
    }
    
    TEST_ASSERT_TRUE(DiagRecorder_ExportToFile("test_export.txt"));
    
    // Verify file contents
    FILE* f = fopen("test_export.txt", "r");
    TEST_ASSERT_NOT_NULL(f);
    
    char buffer[256];
    TEST_ASSERT_TRUE(fgets(buffer, sizeof(buffer), f) != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "Diagnostic Recording Export") != NULL);
    
    fclose(f);
    remove("test_export.txt");
}

void test_DiagRecorder_CustomRecords(void) {
    uint8_t custom_data[] = {0xAA, 0xBB, 0xCC};
    TEST_ASSERT_TRUE(DiagRecorder_AddCustomRecord(0x01, custom_data, sizeof(custom_data)));
    
    TEST_ASSERT_EQUAL(1, DiagRecorder_GetEntryCount());
    
    const DiagRecordEntry* entry = DiagRecorder_GetEntry(0);
    TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_EQUAL(RECORD_TYPE_CUSTOM, entry->type);
    TEST_ASSERT_EQUAL(0x01, entry->data.custom.type);
    TEST_ASSERT_EQUAL_MEMORY(custom_data, entry->data.custom.data, sizeof(custom_data));
}

void test_DiagRecorder_FindSequence(void) {
    uint8_t pattern[] = {0x01, 0x02, 0x03};
    
    // Add entries
    for (uint32_t i = 0; i < 10; i++) {
        DiagRecordEntry entry = {
            .timestamp = mock_get_timestamp(),
            .type = RECORD_TYPE_MESSAGE,
            .sequence = i,
            .data.message = {
                .service_id = 0x10,
                .sub_function = 0x01,
                .data_length = 3,
                .data = {i, i + 1, i + 2}
            }
        };
        add_entry(&entry);
    }
    
    uint32_t index = DiagRecorder_FindSequence(pattern, sizeof(pattern), 0);
    TEST_ASSERT_EQUAL(1, index);
}

void test_DiagRecorder_Statistics(void) {
    // Add entries
    for (uint32_t i = 0; i < 5; i++) {
        DiagRecordEntry entry = {
            .timestamp = mock_get_timestamp(),
            .type = RECORD_TYPE_MESSAGE,
            .sequence = i,
            .data.message = {
                .service_id = 0x10,
                .sub_function = 0x01,
                .data_length = 3,
                .data = {i, i + 1, i + 2}
            }
        };
        add_entry(&entry);
    }
    
    DiagRecorderStats stats;
    DiagRecorder_GetStats(&stats);
    
    TEST_ASSERT_EQUAL(5, stats.message_count);
    TEST_ASSERT_EQUAL(15, stats.total_data_bytes);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagRecorder_BasicRecording);
    RUN_TEST(test_DiagRecorder_CircularBuffer);
    RUN_TEST(test_DiagRecorder_ExportToFile);
    RUN_TEST(test_DiagRecorder_CustomRecords);
    RUN_TEST(test_DiagRecorder_FindSequence);
    RUN_TEST(test_DiagRecorder_Statistics);
    
    return UNITY_END();
} 