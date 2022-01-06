
#include "TimeSync.h"
#include "VideoCommon/OnScreenDisplay.h"

// a lot of this time sync stuff was taken from slippi
// Huge thanks to them <3
// if need be, this can be reworked so it doesn't resemble slippi so much

TimeSync::~TimeSync() {

}

TimeSync::TimeSync() {

}

bool TimeSync::shouldStallFrame(u32 currentFrame, u32 latestRemoteFrame, u8 numPlayers) {
    if (this->isConnectionStalled) return false;

    s32 frameDiff = (s32)currentFrame - (s32)latestRemoteFrame;

    INFO_LOG(BRAWLBACK, "local/remote frame difference - local: %u  remote (wo delay) %u  diff %d\n", currentFrame, latestRemoteFrame - FRAME_DELAY, frameDiff);

    std::stringstream dispStr;
    dispStr << "| Frame diff: " << frameDiff << " |";
    OSD::AddTypedMessage(OSD::MessageType::NetPlayBuffer, dispStr.str(), OSD::Duration::NORMAL, OSD::Color::CYAN);

    // SLIPPI LOGIC
    #if ROLLBACK_IMPL
    if (frameDiff >= MAX_ROLLBACK_FRAMES) { // if we're MAX_ROLLBACK_FRAMES (or more) ahead opponent, timesync (stall)
    #else
    if (frameDiff >= FRAME_DELAY) { 
    #endif
        this->stallFrameCount += 1;
        if (this->stallFrameCount > 60 * 7) {
            isConnectionStalled = true;
        }
        INFO_LOG(BRAWLBACK, "Exceeded rollback lim, halting for one frame. Frame: %u  Latest (wo delay) %u diff %u\n", currentFrame, latestRemoteFrame - FRAME_DELAY, frameDiff);
        return true;
    }
    this->stallFrameCount = 0;

    // Return true if we are over 60% of a frame ahead of our opponent. Currently limiting how
	// often this happens because I'm worried about jittery data causing a lot of unneccesary delays.
	// Only skip once for a given frame because our time detection method doesn't take into consideration
	// waiting for a frame. Also it's less jarring and it happens often enough that it will smoothly
	// get to the right place
	auto isTimeSyncFrame = currentFrame % ONLINE_LOCKSTEP_INTERVAL; // Only time sync every few frames
	if (isTimeSyncFrame == 0 && !this->isSkipping)
	{
		s32 offsetUs = this->calcTimeOffsetUs(numPlayers);
		INFO_LOG(BRAWLBACK, "[Frame %u] Offset is: %d us", currentFrame, offsetUs);

		// TODO: figure out a better solution here for doubles?
		if (offsetUs > 10000)
		{
			this->isSkipping = true;

			int maxSkipFrames = currentFrame <= 120 ? 5 : 1; // On early frames, support skipping more frames
			this->framesToSkip = ((offsetUs - 10000) / MS_IN_FRAME) + 1;
			this->framesToSkip = this->framesToSkip > maxSkipFrames ? maxSkipFrames : this->framesToSkip; // Only skip 5 frames max

			WARN_LOG(BRAWLBACK, "Halting on frame %d due to time sync. Offset: %d us. framesToSkip: %d", currentFrame,
			         offsetUs, this->framesToSkip);
		}
	}

	// Handle the skipped frames
	if (this->framesToSkip > 0)
	{
		// If ahead by 60% of a frame, stall. I opted to use 60% instead of half a frame
		// because I was worried about two systems continuously stalling for each other
		this->framesToSkip -= 1;
		return true;
	}

	this->isSkipping = false;

	return false;
}

// getting frame with delay
void TimeSync::TimeSyncUpdate(u32 frame, u8 numPlayers) {
    u64 currentTime = Common::Timer::GetTimeUs();
    {   // store the time that we sent framedata
        std::lock_guard<std::mutex> lock(this->ackTimersMutex);
        for (int i = 0; i < numPlayers; i++) {
            FrameTiming timing;
            timing.frame = frame;
            timing.timeUs = currentTime;

            this->lastFrameTimings[i] = timing;
            this->ackTimers[i].push_back(timing);
        }
    }
}


// getting frame with delay
void TimeSync::ReceivedRemoteFramedata(s32 frame, u8 playerIdx, bool hasGameStarted) {
    s64 curTime = (s64)Common::Timer::GetTimeUs();
    // update frame timing/offsets for time sync logic
    
    // Pad received, try to guess what our local time was when the frame was sent by our opponent
    // before we initialized
    // We can compare this to when we sent a pad for last frame to figure out how far/behind we
    // are with respect to the opponent

    // SLIPPI LOGIC
    auto timing = this->lastFrameTimings[playerIdx];
    if (!hasGameStarted)
    {
        // Handle case where opponent starts sending inputs before our game has reached frame 1. This will
        // continuously say frame 0 is now to prevent opp from getting too far ahead
        timing.frame = 0;
        timing.timeUs = curTime;
    }

    s64 opponentSendTimeUs = curTime - ((s64)this->pingUs[playerIdx] / 2);
    s64 frameDiffOffsetUs = MS_IN_FRAME * (timing.frame - frame);
    s64 timeOffsetUs = opponentSendTimeUs - timing.timeUs + frameDiffOffsetUs;

    INFO_LOG(BRAWLBACK, "[Offset] Opp Frame: %d, My Frame: %d. Time offset: %lld\n", 
                                              frame-FRAME_DELAY, timing.frame-FRAME_DELAY, timeOffsetUs);

    // Add this offset to circular buffer for use later

    // frameOffsetData is being treated as a "circular buffer". Hence this logic here
    if (this->frameOffsetData[playerIdx].buf.size() < ONLINE_LOCKSTEP_INTERVAL) {
        this->frameOffsetData[playerIdx].buf.push_back((s32)timeOffsetUs);
    }
    else {
        this->frameOffsetData[playerIdx].buf[this->frameOffsetData[playerIdx].idx] = (s32)timeOffsetUs;
    }

    this->frameOffsetData[playerIdx].idx = (this->frameOffsetData[playerIdx].idx + 1) % ONLINE_LOCKSTEP_INTERVAL;
}


// with delay
void TimeSync::ProcessFrameAck(FrameAck* frameAck, u8 numPlayers, const std::array<bool, MAX_NUM_PLAYERS>& hasRemoteInputsThisFrame) {
    u64 currentTime = Common::Timer::GetTimeUs();
    u8 playerIdx = frameAck->playerIdx;
    int frame = frameAck->frame; // this is with frame delay

    // SLIPPI LOGIC

    int lastAcked = this->lastFrameAcked[playerIdx];
    // if this current acked frame is more recent than the last acked frame, set it
    this->lastFrameAcked[playerIdx] = lastAcked < frame ? frame : lastAcked;

    // remove old timings
    while (!this->ackTimers[playerIdx].empty() && this->ackTimers[playerIdx].front().frame < frame) {
        this->ackTimers[playerIdx].pop_front();
    }

    if (this->ackTimers[playerIdx].empty()) {
        INFO_LOG(BRAWLBACK, "Empty acktimers\n");
        return;
    }
    if (this->ackTimers[playerIdx].front().frame != frame) {
        //INFO_LOG(BRAWLBACK, "frontframe and acked frame not equal\n");
        return;
    }

    auto sendTime = this->ackTimers[playerIdx].front().timeUs;

    this->ackTimers[playerIdx].pop_front();

    // our ping is the current gametime - the time that the inputs were originally sent at
    // inputs go from client 1 -> client 2 -> client 2 acks & sends ack to client 1 -> client 1 receives ack here
    // so this is full RTT (round trip time). To get ping just from client to client, divide this by 2
    this->pingUs[playerIdx] = currentTime - sendTime;
    u64 rtt = this->pingUs[playerIdx];
    double rtt_ms = (double)rtt / 1000.0;

    INFO_LOG(BRAWLBACK, "Remote Frame acked %u (w/o delay: %u)  [pIdx %u rtt %f ms]\n", frame, frame-FRAME_DELAY, (unsigned int)playerIdx, rtt_ms);

    if (frame % PING_DISPLAY_INTERVAL == 0) {
        std::stringstream dispStr;
        dispStr << "Has Remote Inputs: ";
        for (int i = 0; i < numPlayers; i++)
            dispStr << hasRemoteInputsThisFrame[i] << " | ";
        dispStr << "Ping: ";
        double ping = rtt_ms / 2.0;
        dispStr << ping << " ms";
        OSD::AddTypedMessage(OSD::MessageType::NetPlayPing, dispStr.str(), OSD::Duration::NORMAL, OSD::Color::GREEN);
    }
}


int TimeSync::getMinAckFrame(u8 numPlayers) {
    int minAckFrame = 0;
    for (int i = 0; i < numPlayers; i++) {
        //INFO_LOG(BRAWLBACK, "lastFrameAcked[%i]: %i", i, this->lastFrameAcked[i]);
        if (minAckFrame == 0 || (this->lastFrameAcked[i] < minAckFrame && this->lastFrameAcked[i] != 0))
            minAckFrame = this->lastFrameAcked[i];
    }
    return minAckFrame;
}


// SLIPPI LOGIC
s32 TimeSync::calcTimeOffsetUs(u8 numPlayers) {
    bool empty = true;
	for (int i = 0; i < numPlayers; i++)
	{
		if (!frameOffsetData[i].buf.empty())
		{
			empty = false;
			break;
		}
	}
	if (empty)
	{
		return 0;
	}

	std::vector<int> offsets;
	for (int pIdx = 0; pIdx < numPlayers; pIdx++)
	{
		if (frameOffsetData[pIdx].buf.empty())
			continue;

		std::vector<s32> buf;
		std::copy(frameOffsetData[pIdx].buf.begin(), frameOffsetData[pIdx].buf.end(), std::back_inserter(buf));

		std::sort(buf.begin(), buf.end());

		int bufSize = (int)buf.size(); // 30
		int offset = (int)((1.0f / 3.0f) * bufSize); // 1/3 bufSize  -- 10
		int end = bufSize - offset; // 30 - 10 = 20

		int sum = 0;
		for (int i = offset; i < end; i++)
		{
			sum += buf[i];
		}

		int count = end - offset; // 20 - 10 = 10
		if (count <= 0)
		{
			return 0;
		}

		s32 result = sum / count;
		offsets.push_back(result);
	}

	s32 maxOffset = offsets.front();
	for (int i = 1; i < offsets.size(); i++)
	{
		if (offsets[i] > maxOffset)
			maxOffset = offsets[i];
	}

    std::stringstream timeOffsets;
    timeOffsets << "Time offsets: ";
    for (s32 o : offsets) {
        timeOffsets << o << " |";
    }
	timeOffsets << " | Max offset: " << maxOffset;
    INFO_LOG(BRAWLBACK, "%s\n", timeOffsets.str().c_str());
	return maxOffset;
}
