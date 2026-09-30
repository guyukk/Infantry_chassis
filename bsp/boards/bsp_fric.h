#ifndef BSP_FRIC_H
#define BSP_FRIC_H
#include "struct_typedef.h"

#define FRIC_UP 1370
//#define FRIC_UP 1400
#define FRIC_DOWN 1320

#define FRIC_OFF 1000

//旋转到0度，关弹舱
#define Bomb_bay_off 500  
//旋转到90度，开弹舱
#define Bomb_bay_on 1300 

extern void fric_off(void);
extern void fric1_on(uint16_t cmd);
extern void fric2_on(uint16_t cmd);
extern void Bombbay_on(void);
extern void Bombbay_off(void);
#endif
