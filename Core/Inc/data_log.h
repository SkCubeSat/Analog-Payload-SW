extern uint16_t data_log[DATA_LOG_CAPACITY];
extern uint16_t data_count;

void data_log_push(uint16_t value);
void data_log_clear(void);
