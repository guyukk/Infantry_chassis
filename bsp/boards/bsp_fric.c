#include "bsp_fric.h"
#include "cmsis_os.h"
#include "main.h"
extern TIM_HandleTypeDef htim1;

void fric_off(void)
{
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, FRIC_OFF);
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_2, FRIC_OFF);
}
void fric1_on(uint16_t cmd)
{
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_1, cmd);
}
void fric2_on(uint16_t cmd)
{
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_2, cmd);
}

void Bombbay_off(void)
{
		__HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_3, Bomb_bay_off);
//		for(int i = Bomb_bay_on ; i > Bomb_bay_off ; i -- ){
//			__HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_3, i);
//			vTaskDelay(1);
//		}
}

void Bombbay_on(void)
{
		__HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_3, Bomb_bay_on);
//    for(int i = Bomb_bay_off ; i < Bomb_bay_on ; i ++ ){
//			__HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_3, i);
//			vTaskDelay(1);
//		}
}
