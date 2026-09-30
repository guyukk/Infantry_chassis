/**
	* 图传模块的英文名称叫 Video_Transmission_Module
	*/

#include <string.h>
#include "video_transmission_module.h"

// 全局图传数据实例
VTM_Data_t vtm_data;

/**
 * @brief  搜索合法帧头位置
 * @param  buf: 接收缓冲区指针
 * @param  buf_len: 接收缓冲区总长度
 * @retval 合法帧头起始索引，-1表示未找到
 * @note   帧头为连续的REMOTE_HEAD1(0xA9)+REMOTE_HEAD2(0x53)，需保证后续有REMOTE_FRAME_LEN字节有效数据
 */
static int16_t vtm_search_head(uint8_t *buf, uint16_t buf_len)
{
    int16_t i;
    // 遍历缓冲区，预留整帧长度，避免后续复制数据越界
    for(i = 0; i < 20; i++)
    {
        if((buf[i] == REMOTE_HEAD1) && (buf[i+1] == REMOTE_HEAD2))
        {
            return i; // 找到合法帧头，返回起始索引
        }
    }
    return -1; // 未找到帧头
}

/**
 * @brief  遥控数据解包函数
 * @param  buf: 接收缓冲区指针
 * @param  buf_len: 接收缓冲区实际长度
 * @param  data: 解包后数据存储结构体
 * @retval 1:解包成功 0:解包失败(未找到帧头/缓冲区长度不足)
 * @note   找到帧头后复制有效数据到临时数组再解包，彻底避免越界；不处理CRC校验
 */
uint8_t vtm_data_unpack(uint8_t *buf, uint16_t buf_len, VTM_Data_t *data)
{
    int16_t head_index = -1;
    uint8_t temp_byte;
    // 定义临时数组存储整帧有效数据，避免操作原缓冲区越界
    uint8_t frame_buf[REMOTE_FRAME_LEN] = {0};

    // 1. 校验缓冲区长度，至少需容纳一帧数据
    if(buf_len < REMOTE_FRAME_LEN || buf == NULL || data == NULL)
    {
        return 0;
    }

    // 2. 搜索合法帧头位置
    head_index = vtm_search_head(buf, buf_len);
    if(head_index == -1)
    {
        return 0; // 未找到帧头，解包失败
    }

    // 3. 从帧头起始位置复制21字节有效数据到临时数组
		memcpy(frame_buf, buf + head_index, REMOTE_FRAME_LEN);

    // 4. 基于临时数组解析整帧数据，与原协议位解析逻辑完全一致
    // 解析帧头
    data->head1 = frame_buf[0];
    data->head2 = frame_buf[1];
    // 解析4个11bit通道(ch0~ch3)
    data->ch0 = (	(uint16_t)frame_buf[2] 				 | ( (uint16_t)frame_buf[3] << 8));
    data->ch1 = (((uint16_t)(frame_buf[3] >> 3)) | ( (uint16_t)frame_buf[4] << 5));
    data->ch2 = (((uint16_t)(frame_buf[4] >> 6)) | ( (uint16_t)frame_buf[5] << 2) | ( (uint16_t)frame_buf[6] << 10));
    data->ch3 = (((uint16_t)(frame_buf[6] >> 1)) | ( (uint16_t)frame_buf[7] << 7));
    // 解析档位开关(2bit)+ 3个1bit按键
    temp_byte = frame_buf[7];
    data->gear_switch 	= (frame_buf[7] >> 4);    // bit0~1: 档位
    data->pause_key   	= (frame_buf[7] >> 6);		// bit2: 暂停键
    data->custom_l_key 	= (frame_buf[7] >> 7);		// bit3: 自定义左键
    data->custom_r_key 	=  frame_buf[8];					// bit4: 自定义右键
    // 解析拨轮(11bit)
    data->wheel = ((uint16_t)frame_buf[8] >> 1) | ((uint16_t)frame_buf[9] << 7);
    // 解析扳机键(1bit)
    data->trigger_key = ((uint16_t)frame_buf[9] >> 4);
    // 解析鼠标三轴(3个16bit有符号数，小端序)
    data->mouse_x = (int16_t)( (uint16_t)frame_buf[10] | ( (uint16_t)frame_buf[11] << 8));
    data->mouse_y = (int16_t)( (uint16_t)frame_buf[12] | ( (uint16_t)frame_buf[13] << 8));
    data->mouse_z = (int16_t)( (uint16_t)frame_buf[14] | ( (uint16_t)frame_buf[15] << 8));
    // 解析鼠标按键(3个2bit)
    temp_byte = frame_buf[16];
    data->mouse_l_key = temp_byte;         // bit0~1: 左键
    data->mouse_r_key = (temp_byte >> 2);  // bit2~3: 右键
    data->mouse_m_key = (temp_byte >> 4);  // bit4~5: 中键
    // 解析键盘按键(16bit，小端序)
    data->keyboard_key = (uint16_t)( (uint16_t)frame_buf[17] | ( (uint16_t)frame_buf[18] << 8));
    // 解析CRC(仅占位，不处理校验，无越界)
    data->crc = (uint16_t)(frame_buf[19]) | (uint16_t)(frame_buf[20] << 8);

    return 1; // 解包成功
}

/**
 * @brief  遥控数据初始化
 * @param  无
 * @retval 无
 * @note   初始化全局结构体为默认安全值
 */
void vtm_data_init(void)
{
    // 初始化结构体为默认值，保留原初始化逻辑
    vtm_data.head1 = REMOTE_HEAD1;
    vtm_data.head2 = REMOTE_HEAD2;
    vtm_data.ch0 = CHANNEL_MID;
    vtm_data.ch1 = CHANNEL_MID;
    vtm_data.ch2 = CHANNEL_MID;
    vtm_data.ch3 = CHANNEL_MID;
    vtm_data.gear_switch = GEAR_N;
    vtm_data.pause_key = KEY_RELEASE;
    vtm_data.custom_l_key = KEY_RELEASE;
    vtm_data.custom_r_key = KEY_RELEASE;
    vtm_data.wheel = CHANNEL_MID;
    vtm_data.trigger_key = KEY_RELEASE;
    vtm_data.mouse_x = 0;
    vtm_data.mouse_y = 0;
    vtm_data.mouse_z = 0;
    vtm_data.mouse_l_key = KEY_RELEASE;
    vtm_data.mouse_r_key = KEY_RELEASE;
    vtm_data.mouse_m_key = KEY_RELEASE;
    vtm_data.keyboard_key = 0;
    vtm_data.crc = 0;
}
