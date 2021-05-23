

#ifndef _SERIAL_OUT_H_
#define _SERIAL_OUT_H_

#ifdef __cplusplus
 extern "C" {
#endif

void serial_init(void);

void serial_send(const char* buffer, int count);

#ifdef __cplusplus
 }
#endif

#endif /* _SERIAL_OUT_H_ */
