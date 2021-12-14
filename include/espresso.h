#ifdef __cplusplus
extern "C" {
#endif

void wifi_init_sta(void);
void mqtt_init(void);

void mqtt_publishf(const char *topic, float value);
void mqtt_publishs(const char *topic, const char *value);

void temperature_target_set(float value);

#ifdef __cplusplus
}
#endif
