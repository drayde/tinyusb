

#ifndef _SERIAL_OUT_H_
#define _SERIAL_OUT_H_

#ifdef __cplusplus
 extern "C" {
#endif

void serial_init(void);

uint16_t serial_send(const uint8_t* buffer, uint16_t count);

#ifdef __cplusplus
 }
#endif

#endif /* _SERIAL_OUT_H_ */
