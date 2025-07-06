//
// Created by Admin on 2025/6/30.
//

#ifndef DEMUX_H
#define DEMUX_H
#include "PlayerContext.h"


class PacketDemux {
public:
    PacketDemux(PlayerContext *ctx);
    ~PacketDemux();
    int demux();
    int stopDemux();

private:
    void demuxPacket();

private:
    PlayerContext *player_ctx = nullptr;
    std::thread *demux_thread = nullptr;
};



#endif //DEMUX_H
