#ifndef _FMUSIC_XAUDIO2_CFG_H_
#define _FMUSIC_XAUDIO2_CFG_H_

#define USE_XAUDIO2_ENGINE 1	// 是否使用XAUDIO2作为声音输出(如果关闭，则使用WaveOut进行输出，在360上将无法运行)
#define USE_XAUDIO2_LOOP   1    // 是否使用XAUDIO2自带的单缓冲循环机制，由MiniFmod往该缓冲中循环写入数据(建议打开，如果关闭，则使用多缓冲顺序提交播放机制，因为用了多缓冲，导致不能正确获取当前播放的order/row/ms信息)

#endif