

#include "EXIBrawlback.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include <chrono>
#include <iostream>
#include "VideoCommon/OnScreenDisplay.h"
#include <climits>
#include <fstream>
#include <filesystem>
#include <vector>
#include <regex>
#include <Core/Brawlback/include/brawlback-common/ExiStructures.h>
#include "UICommon/UICommon.h"
namespace fs = std::filesystem;

// --- Mutexes
std::mutex read_queue_mutex = std::mutex();
std::mutex remotePadQueueMutex = std::mutex();
std::mutex localPadQueueMutex = std::mutex();
// -------------------------------

template <class T>
[[nodiscard]] T swap_endian(T val)
{
  union U
  {
    T val;
    std::array<std::uint8_t, sizeof(T)> raw;
  } src, dst;

  src.val = val;
  std::reverse_copy(src.raw.begin(), src.raw.end(), dst.raw.begin());
  val = dst.val;
  return val;
}
void writeToFile(std::string filename, uint8_t* ptr, size_t len)
{
  std::ofstream fp;
  fp.open(filename, std::ios::out | std::ios::binary);
  fp.write((char*)ptr, len);
}
std::vector<u8> read_vector_from_disk(std::string file_path)
{
  std::ifstream instream(file_path, std::ios::in | std::ios::binary);
  std::vector<u8> data((std::istreambuf_iterator<char>(instream)),
                       std::istreambuf_iterator<char>());
  return data;
}

CEXIBrawlback::CEXIBrawlback()
{
  INFO_LOG(BRAWLBACK, "------- %s\n", SConfig::GetInstance().GetGameID().c_str());
#ifdef _WIN32
  if (std::filesystem::exists(Sync::getSyncLogFilePath()))
  {
    std::filesystem::remove(Sync::getSyncLogFilePath());
  }
    if (std::filesystem::exists(File::GetExeDirectory() + "User/Logs/dolphin.log")) {
        std::filesystem::remove(File::GetExeDirectory() + "User/Logs/dolphin.log");
    }
#endif

  INFO_LOG(BRAWLBACK, "BRAWLBACK exi ctor");
  // TODO: initialize this only when finding matches
  auto enet_init_res = enet_initialize();
  if (enet_init_res < 0)
  {
    ERROR_LOG(BRAWLBACK, "Failed to init enet! %d\n", enet_init_res);
  }
  else if (enet_init_res == 0)
  {
    INFO_LOG(BRAWLBACK, "Enet init success");
  }
  this->netplay = std::make_unique<BrawlbackNetplay>();
  this->matchmaking = std::make_unique<Matchmaking>(this->getUserInfo());
  this->timeSync = std::make_unique<TimeSync>();
}

Brawlback::UserInfo CEXIBrawlback::getUserInfo()
{
  Brawlback::UserInfo info;

  std::string lylat;
#ifdef _WIN32
  std::string lylat_json_path = File::GetExeDirectory() + "/lylat.json";
#else
  // This will look on "~/Libraries/Application Support/Dolphin/lylat.json" on macosx
  std::string lylat_json_path = File::GetUserPath(D_USER_IDX) + "lylat.json";
#endif
  INFO_LOG(BRAWLBACK, "Reading lylat json from %s\n", lylat_json_path.c_str());
  std::string data;
  if (!File::ReadFileToString(lylat_json_path, data))
  {
    ERROR_LOG(BRAWLBACK, "Could not find lylat.json.");
    return info;
  }

  json j = json::parse(data);
  INFO_LOG(BRAWLBACK, "JSON Contents: %s", j.dump(4).c_str());

  info.uid = j["uid"].get<std::string>();
  info.playKey = j["playKey"].get<std::string>();
  info.connectCode = j["connectCode"].get<std::string>();
  info.displayName = "Dev Test";
  info.latestVersion = "test";
  info.fileContents = "test";

  return info;
}

CEXIBrawlback::~CEXIBrawlback()
{
  enet_deinitialize();
  enet_host_destroy(this->server);
  this->isConnected = false;
  if (this->netplay_thread.joinable())
  {
    this->netplay_thread.join();
  }

  delete this->matchmaking.release();
  if (this->matchmaking_thread.joinable())
  {
    this->matchmaking_thread.join();
  }
}



void CEXIBrawlback::handleCaptureSavestate(u8* data)
{

  u64 startTime = Common::Timer::GetTimeUs();

  int idx = 0;
  s32 frame = (s32)SlippiUtility::Mem::readWord(data, idx, 999, 0);

  if (frame % 30 == 0)
  {
    static int dump_num = 0;
    // INFO_LOG(BRAWLBACK, "Dumping mem\n");
    // Dump::DoMemDumpIteration(dump_num);
  }

  // tmp
  if (frame == GAME_START_FRAME && availableSavestates.empty())
  {
    activeSavestates.clear();
    availableSavestates.clear();
    for (int i = 0; i <= MAX_ROLLBACK_FRAMES; i++)
    {
      availableSavestates.push_back(std::make_unique<BrawlbackSavestate>());
    }
    INFO_LOG(BRAWLBACK, "Initialized savestates!\n");
  }

  // Grab an available savestate
  std::unique_ptr<BrawlbackSavestate> ss;
  if (!availableSavestates.empty())
  {
    ss = std::move(availableSavestates.back());
    availableSavestates.pop_back();
  }
  else
  {
    // If there were no available savestates, use the oldest one
    auto it = activeSavestates.begin();
    ss = std::move(it->second);
    // s32 frameToDrop = it->first;
    // INFO_LOG(BRAWLBACK, "Dropping savestate for frame %i\n", frameToDrop);
    activeSavestates.erase(it->first);
  }

  // If there is already a savestate for this frame, remove it and add it to available
  if (!activeSavestates.empty() && activeSavestates.count(frame))
  {
    availableSavestates.push_back(std::move(activeSavestates[frame]));
    activeSavestates.erase(frame);
  }

  if (!ss)
  {
    ERROR_LOG(BRAWLBACK, "Invalid savestate on frame %i\n", frame);
    PanicAlertFmtT("Invalid savestate on frame {0}\n", frame);
    return;
  }

  ss->Capture();
  // ss->frame = frame;
  // ss->checksum = SavestateChecksum(ss->getBackupLocs()); // really expensive calculation
  // INFO_LOG(BRAWLBACK, "Savestate for frame %i checksum is %i\n", ss->frame, ss->checksum);
  activeSavestates[frame] = std::move(ss);

  u32 timeDiff = (u32)(Common::Timer::GetTimeUs() - startTime);
  INFO_LOG(BRAWLBACK, "Captured savestate for frame %d in: %f ms", frame,
           ((double)timeDiff) / 1000);
}

void CEXIBrawlback::handleLoadSavestate(u8* data)
{

  RollbackInfo* loadStateRollbackInfo = (RollbackInfo*)data;
  // the frame we should load state for is the frame we first began not receiving inputs
  // u32 frame = Common::swap32(loadStateRollbackInfo->beginFrame);
  s32 frame = (s32)SlippiUtility::Mem::readWord((u8*)&loadStateRollbackInfo->beginFrame);

  // INFO_LOG(BRAWLBACK, "Attempting to load state for frame %i\n", frame);

  // u32* preserveArr = (u32*)loadStateRollbackInfo->preserveBlocks.data();

  if (!activeSavestates.count(frame))
  {
    // This savestate does not exist - just disconnect and throw hands :/
    ERROR_LOG(BRAWLBACK, "Savestate for frame %i does not exist.", frame);
    PanicAlertFmtT("Savestate for frame {0} does not exist.", frame);
    this->isConnected = false;
    if (this->server)
    {
      for (int i = 0; i < this->server->peerCount; i++)
      {
        enet_peer_disconnect(&this->server->peers[i], 0);
      }
    }
    // in the future, exit out of the match or something here
    return;
  }

  u64 startTime = Common::Timer::GetTimeUs();

  // Fetch preservation blocks
  std::vector<PreserveBlock> blocks = {};

  /*
// Get preservation blocks
int idx = 0;
while (Common::swap32(preserveArr[idx]) != 0)
{
  PreserveBlock p = {Common::swap32(preserveArr[idx]), Common::swap32(preserveArr[idx + 1])};
  blocks.push_back(p);
  idx += 2;
}
  */

  // Load savestate
  activeSavestates[frame]->Load(blocks);

  // Move all active savestates to available
  for (auto it = activeSavestates.begin(); it != activeSavestates.end(); ++it)
  {
    availableSavestates.push_back(std::move(it->second));
  }

  // since we save state during resim frames, when we load state,
  // we should clear all savestates out to make room for the new resimulated states

  activeSavestates.clear();

  u32 timeDiff = (u32)(Common::Timer::GetTimeUs() - startTime);
  ERROR_LOG(BRAWLBACK, "Loaded savestate for frame %d in: %f ms", frame, ((double)timeDiff) / 1000);
}

template <typename T>
void CEXIBrawlback::SendCmdToGame(EXICommand cmd, T* payload)
{
  // std::lock_guard<std::mutex> lock (read_queue_mutex); // crash here wtf?
  this->read_queue.clear();
  this->read_queue.push_back(cmd);
  if (payload)
  {
    std::vector<u8> byteVec = Mem::structToByteVector(payload);
    this->read_queue.insert(this->read_queue.end(), byteVec.begin(), byteVec.end());
  }
}

void CEXIBrawlback::SendCmdToGame(EXICommand cmd)
{
  // std::lock_guard<std::mutex> lock (read_queue_mutex);
  this->read_queue.clear();
  this->read_queue.push_back(cmd);
}

PlayerFrameData CreateDummyPlayerFrameData(u32 frame, u8 playerIdx)
{
  PlayerFrameData dummy_framedata = PlayerFrameData();
  dummy_framedata.frame = frame;
  dummy_framedata.playerIdx = playerIdx;
  dummy_framedata.pad = BrawlbackPad();  // empty pad
  return dummy_framedata;
}

// `data` is a ptr to a PlayerFrameData struct
// this is called every frame at the beginning of the frame
void CEXIBrawlback::handleLocalPadData(u8* data)
{
  PlayerFrameData playerFramedata;
  std::memcpy(&playerFramedata, data, sizeof(PlayerFrameData));
  int idx = 0;
  // first 4 bytes are current game frame
  u32 frame = SlippiUtility::Mem::readWord(data, idx, 999, 0);  // properly switched endianness
  u8 playerIdx = playerFramedata.playerIdx;
  playerFramedata.frame = frame;  // properly switched endianness

  if (frame == GAME_START_FRAME)
  {
    // push framedatas for first few delay frames
    for (int i = GAME_START_FRAME; i < FRAME_DELAY; i++)
    {
      this->remotePlayerFrameData[playerIdx].push_back(
          std::make_unique<PlayerFrameData>(CreateDummyPlayerFrameData(i, playerIdx)));
      this->localPlayerFrameData.push_back(
          std::make_unique<PlayerFrameData>(CreateDummyPlayerFrameData(i, playerIdx)));
    }
    this->hasGameStarted = true;
  }

  static int numTimesyncs = 0;
  if (frame % 60 == 0)
  {
    OSD::AddTypedMessage(OSD::MessageType::NetPlayBuffer,
                         "Timesyncs: " + std::to_string(numTimesyncs) + "\n", OSD::Duration::NORMAL,
                         OSD::Color::CYAN);
    numTimesyncs = 0;
  }

  if (frame % PING_DISPLAY_INTERVAL == 0)
    OSD::AddTypedMessage(
        OSD::MessageType::BrawlbackBuffer,
        "Time offset: " +
            std::to_string((double)timeSync->calcTimeOffsetUs(this->numPlayers) / 1000) + " ms\n");

  int remote_frame = (int)this->GetLatestRemoteFrame();
  bool shouldTimeSync = this->timeSync->shouldStallFrame(frame, remote_frame, this->numPlayers);
  if (shouldTimeSync)
  {
    INFO_LOG(BRAWLBACK, "Should time sync\n");
    // Send inputs that have not yet been acked
    this->handleSendInputs(frame);
    this->SendCmdToGame(EXICommand::CMD_TIMESYNC);
    numTimesyncs += 1;
  }
  else
  {
    // store these local inputs (with frame delay)
    this->storeLocalInputs(&playerFramedata);
    // broadcasts local inputs
    this->handleSendInputs(frame);

    // getting inputs for all players & sending them to game
    FrameData framedataToSendToGame = FrameData(frame);
    framedataToSendToGame.randomSeed = 0x496ffd00;  // tmp
    // populates playerFrameDatas field of the above FrameData
    std::pair<bool, bool> foundData = this->getInputsForGame(framedataToSendToGame, frame);

#if ROLLBACK_IMPL
    if (this->rollbackInfo.pastFrameDataPopulated)
    {
      this->SendCmdToGame(EXICommand::CMD_ROLLBACK, &this->rollbackInfo);
      ResetRollbackInfo(this->rollbackInfo);  // reset rollbackInfo
      return;
    }
#endif

    if (!foundData.second && frame > FRAME_DELAY)
    {  // if we didn't find remote inputs AND didn't find/use predicted inputs for some reason
      INFO_LOG(BRAWLBACK, "Couldn't find any remote inputs - Sending time sync command\n");
      this->SendCmdToGame(EXICommand::CMD_TIMESYNC);
      numTimesyncs += 1;
    }
    else
    {
#ifdef SYNCLOG
      if (!rollbackInfo.isUsingPredictedInputs)
      {
        Sync::SyncLog(Sync::stringifyFramedata(framedataToSendToGame, 2));
      }
#endif
      this->SendCmdToGame(EXICommand::CMD_FRAMEDATA, &framedataToSendToGame);
            
            // for checking desyncs
            //for (int i = 0; i < 2; i++) {
            //    Sync::SyncLog(Sync::stringifyFramedata(framedataToSendToGame.playerFrameDatas[i]));
            //}

    }

  }
}

void CEXIBrawlback::storeLocalInputs(PlayerFrameData* localPlayerFramedata)
{
  std::lock_guard<std::mutex> local_lock(localPadQueueMutex);
  std::unique_ptr<PlayerFrameData> pFD =
      std::make_unique<PlayerFrameData>(*localPlayerFramedata);
  // local inputs offset by FRAME_DELAY to mask latency
  // Once we hit frame X, we send inputs for that frame, but pretend they're from frame X+2
  // so those inputs now have an extra 2 frames to get to the opponent before the opponent's
  // client hits frame X+2.
  pFD->frame += FRAME_DELAY;
  // INFO_LOG(BRAWLBACK, "Frame %u PlayerIdx: %u numPlayers %u\n", localPlayerFramedata->frame,
  // localPlayerFramedata->playerIdx, this->numPlayers);

  // make sure we're storing inputs sequentially
  if (!this->localPlayerFrameData.empty() &&
      localPlayerFramedata->frame == this->localPlayerFrameData.back()->frame + 1)
  {
    WARN_LOG(BRAWLBACK, "Didn't push local framedata for frame %u\n", pFD->frame);
        return;
  }
  else
  {
    // store local framedata
    if (this->localPlayerFrameData.size() + 1 > FRAMEDATA_MAX_QUEUE_SIZE)
    {
      // INFO_LOG(BRAWLBACK, "Popping local framedata for frame %u\n",
      // this->localPlayerFrameData.front()->frame);
      this->localPlayerFrameData.pop_front();
    }
    this->localPlayerFrameData.push_back(std::move(pFD));
  }
}

void CEXIBrawlback::handleSendInputs(u32 frame)
{
  // broadcast this local framedata
  if (!this->localPlayerFrameData.empty())
  {
    std::lock_guard<std::mutex> local_lock(localPadQueueMutex);

        // this strat is taken from slippi [ thanks fizzi <3 ]
    // each frame we send local inputs to the other client(s)
    // those clients then acknowledge those inputs and send that ack(nowledgement)
    // back to us. All acked inputs are irrelevant (unless we need to rollback)
    // since we know for a fact the remote client has them.
    // We track what frame of inputs has been acked ("localHeadFrame")
    // so that when we go to send inputs, we only send the un-acked ones.
    // We send *all* unacked inputs so that when the remote client doesn't receive inputs, and needs
    // to rollback the next packet will have all the inputs that that client hasn't received.
    int minAckFrame = this->timeSync->getMinAckFrame(this->numPlayers);

    // clamp to current frame to prevent it dropping local inputs that haven't been used yet
    minAckFrame = MIN(minAckFrame, frame);

    int localPadQueueSize = (int)this->localPlayerFrameData.size();
    if (localPadQueueSize == 0)
      return;  // no inputs, nothing to send
    int endIdx = localPadQueueSize - 1 - (this->localPlayerFrameData.back()->frame - minAckFrame);

    std::vector<PlayerFrameData*> localFramedatas = {};
    // push framedatas from back to front
    // this means the resulting vector (localFramedatas) will have the most
    // recent framedata in the first position, and the oldest framedata in the last position
    for (int i = localPadQueueSize - 1; i > endIdx; i--)
    {
      const auto& localFramedata = this->localPlayerFrameData[i];
      // make sure we queue up these inputs sequentially
      if (localFramedatas.empty() ||
          (!localFramedatas.empty() && localFramedatas.back()->frame > localFramedata->frame))
      {
        PlayerFrameData* inputToSend = localFramedata.get();
        localFramedatas.push_back(inputToSend);
      }
    }

    // INFO_LOG(BRAWLBACK, "Broadcasting %i framedatas\n", localFramedatas.size());
    this->netplay->BroadcastPlayerFrameDataWithPastFrames(this->server, localFramedatas);

    u32 mostRecentFrame = this->localPlayerFrameData.back()->frame;  // current frame with delay
    this->timeSync->TimeSyncUpdate(mostRecentFrame, this->numPlayers);

  }
}

std::pair<bool, bool> CEXIBrawlback::getInputsForGame(FrameData& framedataToSendToGame,
                                                      u32 frame)
{
  std::lock_guard<std::mutex> lock(remotePadQueueMutex);

    std::lock_guard<std::mutex> lock (remotePadQueueMutex);

  // first is if we've found local inputs, second is if we've found remote inputs
  std::pair<bool, bool> foundData = std::make_pair(false, false);

  // for each player
  for (int playerIdx = 0; playerIdx < this->numPlayers; playerIdx++)
  {
    // --------- search for local player's inputs -------------
    if (playerIdx == this->localPlayerIdx && !this->localPlayerFrameData.empty())
    {
      std::lock_guard<std::mutex> local_lock(localPadQueueMutex);

      PlayerFrameData* localFrameData =
          findInPlayerFrameDataQueue(this->localPlayerFrameData, frame);
      foundData.first = localFrameData != nullptr;
      if (localFrameData)
      {
        framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] = *localFrameData;
      }

      if (!foundData.first)
      {
        // this shouldn't ever happen lol. Just putting this here so things don't go totally haywire
        ERROR_LOG(BRAWLBACK, "Couldn't find local inputs! Using empty pad.\n");
        WARN_LOG(BRAWLBACK, "Local pad input range: [%u - %u]\n",
                 this->localPlayerFrameData.back()->frame,
                 this->localPlayerFrameData.front()->frame);
        framedataToSendToGame.playerFrameDatas[this->localPlayerIdx] =
            CreateDummyPlayerFrameData(frame, this->localPlayerIdx);
      }
      continue;

    }
    // ------------------------------------------

    // -------- search for remote player's inputs --------
    if (!this->remotePlayerFrameData.empty() && !this->remotePlayerFrameData[playerIdx].empty())
    {
      // find framedata in queue that has the frame we want to inject into the game (current frame -
      // frame delay)
      // INFO_LOG(BRAWLBACK, "Remote framedata q range: %u - %u\n",
      // this->remotePlayerFrameData[playerIdx].front()->frame,
      // this->remotePlayerFrameData[playerIdx].back()->frame);

      PlayerFrameData* remoteFrameData =
          findInPlayerFrameDataQueue(this->remotePlayerFrameData[playerIdx], frame);
      foundData.second = remoteFrameData != nullptr;
      if (remoteFrameData)
      {
        framedataToSendToGame.playerFrameDatas[playerIdx] = *remoteFrameData;
// we've just received this frame's inputs, so if we were using predicted inputs, we now need to
// rollback
#if ROLLBACK_IMPL
        if ((this->rollbackInfo.isUsingPredictedInputs ||
             this->latestConfirmedFrame < (int)frame) &&
            frame > GAME_FULL_START_FRAME)
        {
          this->SetupRollback(frame, frame);
        }
#else
        if (false)
        {
        }
#endif
        else
        {
          INFO_LOG(BRAWLBACK, "found remote inputs frame %u\n", frame);
          this->latestConfirmedFrame = (int)frame;
        }
      }

    }
    // --------------------------------------------------

#if ROLLBACK_IMPL
    if (!foundData.second && frame > GAME_FULL_START_FRAME)
    {  // didn't find framedata for this frame.
      INFO_LOG(BRAWLBACK, "no remote framedata - frame %u remotePIdx %i\n", frame, playerIdx);
      if (this->remotePlayerFrameData[playerIdx].size() >= MAX_ROLLBACK_FRAMES)
      {
        PlayerFrameData* justRecvdInputs = nullptr;
        for (s32 i = (s32)frame; i >= latestConfirmedFrame + 1 && !justRecvdInputs; i--)
        {
          justRecvdInputs = findInPlayerFrameDataQueue(this->remotePlayerFrameData[playerIdx], i);
        }

        if (justRecvdInputs && latestConfirmedFrame >= GAME_FULL_START_FRAME)
        {
          INFO_LOG(BRAWLBACK, "Found past remote inputs for rollback. Frame %i rbBeginFrame: %u",
                   frame, rollbackInfo.beginFrame);
          this->SetupRollback(frame, justRecvdInputs->frame);
        }
        else
        {
          if (!this->rollbackInfo.isUsingPredictedInputs)
          {
            std::optional<PlayerFrameData> predictedInputs =
                this->HandleInputPrediction(frame, playerIdx);
            if (predictedInputs)
            {
              foundData.second = true;
              framedataToSendToGame.playerFrameDatas[playerIdx] = predictedInputs.value();
            }
          }
          else
          {
            PlayerFrameData predictedInputs =
                this->rollbackInfo.predictedInputs.playerFrameDatas[playerIdx];
            if (frame - predictedInputs.frame < MAX_ROLLBACK_FRAMES)
            {
              INFO_LOG(BRAWLBACK, "Using predicted inputs from frame %u\n", predictedInputs.frame);
              framedataToSendToGame.playerFrameDatas[playerIdx] = predictedInputs;
              foundData.second = true;

                    int numFramesWithoutRemoteInputs = frame - predictedInputs.frame; // this is >= 2
                    u32 pastLocalInputDesiredFrame = predictedInputs.frame + numFramesWithoutRemoteInputs;

                    // if we've been predicting for more than max rollback frames
                    if (numFramesWithoutRemoteInputs >= MAX_ROLLBACK_FRAMES) {
                        INFO_LOG(BRAWLBACK, "Num frames without remote inputs exceedes max rollback frames\n");
                        // let parent func know that we "havent found remote inputs" so it'll timesync
                        foundData.second = false;
                    }
                    if (numFramesWithoutRemoteInputs <= MAX_ROLLBACK_FRAMES) {
                        // while we're using predicted inputs, we want to keep track of local inputs so that if a rollback happens
                        // we can resimulate with local inputs as well as remote ones

                        bool found = false;
                        // find local inputs with desired frame
                        PlayerFrameData* localInputs = findInPlayerFrameDataQueue(this->localPlayerFrameData, pastLocalInputDesiredFrame);
                        if (localInputs) {
                            this->rollbackInfo.pastFrameDatas[numFramesWithoutRemoteInputs-1].playerFrameDatas[this->localPlayerIdx] = *localInputs;
                            INFO_LOG(BRAWLBACK, "Found past local input for rollback frame %u. inserting to idx %i\n", localInputs->frame, numFramesWithoutRemoteInputs-1);
                            found = true;
            }
            else
            {
              WARN_LOG(BRAWLBACK,
                       "Prediction threshold reached! Predicted frame %u  current frame %u \n",
                       predictedInputs.frame, frame);
              // will timesync
            }

          }


        }

      }
      else
      {
        ERROR_LOG(BRAWLBACK, "Too early of a frame. Can't rollback. Using dummy pad\n");
        framedataToSendToGame.playerFrameDatas[playerIdx] =
            CreateDummyPlayerFrameData(frame, playerIdx);
      }
    }
#else
    if (!foundData.second)
    {  // didn't find framedata for this frame.
      ERROR_LOG(BRAWLBACK, "no remote framedata - frame %u remotePIdx %i\n", frame, playerIdx);
      framedataToSendToGame.playerFrameDatas[playerIdx] =
          CreateDummyPlayerFrameData(frame, playerIdx);
    }
#endif
  }

  return foundData;
}

std::optional<PlayerFrameData> CEXIBrawlback::HandleInputPrediction(u32 frame, u8 playerIdx)
{
  std::optional<PlayerFrameData> ret = std::nullopt;

  INFO_LOG(BRAWLBACK, "Trying to find frame for predicted inputs...\n");

  // do we even need a loop here to find the inputs for prediction? I think they should always be
  // the latest confirmed frame
  for (s32 frameIter = this->latestConfirmedFrame; frameIter < (int)frame; frameIter++)
  {
    // find most recent frame that exists
    PlayerFrameData* predictedInputs =
        findInPlayerFrameDataQueue(this->remotePlayerFrameData[playerIdx], frameIter);
    if (predictedInputs)
    {
      INFO_LOG(BRAWLBACK, "found frame for predicting inputs %i\n", frameIter);

      ret = std::make_optional(*predictedInputs);

      s32 rollbackBeginFrame = frameIter + 1;
      // set rollback info

      // we are currently predicting
      this->rollbackInfo.isUsingPredictedInputs = true;
      // we begin our "rollback state" on the latest confirmed remote frame + 1
      this->rollbackInfo.beginFrame = rollbackBeginFrame;
      // store predicted inputs
      this->rollbackInfo.predictedInputs.playerFrameDatas[playerIdx] = *predictedInputs;

      break;
    }
  }

  if (!ret)
  {
    INFO_LOG(BRAWLBACK, "Searched [%i, %i)   remote framedata range: %u - %u\n",
             this->latestConfirmedFrame, frame,
             this->remotePlayerFrameData[playerIdx].front()->frame,
             this->remotePlayerFrameData[playerIdx].back()->frame);
    // couldn't find relevant past framedata
    // this probably means the difference between clients is greater than MAX_ROLLBACK_FRAMES
    // foundData.second will be false, so this should time-sync later in handleLocalPadData
    WARN_LOG(BRAWLBACK, "Couldn't find framedata when we should rollback!! Will timesync\n");
  }

  return ret;
}

// prepares RollbackInfo struct with relevant rollback info
// currentFrame is the current frame we're on, and confirmFrame is the latest frame of inputs we
// just received
void CEXIBrawlback::SetupRollback(u32 currentFrame, u32 confirmFrame)
{
  rollbackInfo.beginFrame = latestConfirmedFrame + 1;  // frame to rollback to
  this->latestConfirmedFrame = (int)confirmFrame;      // frame of latest confirmed remote inputs
  this->rollbackInfo.endFrame = currentFrame;          // current frame we need to resim up to

  INFO_LOG(BRAWLBACK, "Rollback confirmed frame range: [%u, %u] currentFrame: %u  localPidx %i\n",
           this->rollbackInfo.beginFrame, confirmFrame, currentFrame, this->localPlayerIdx);

  for (u32 i = this->rollbackInfo.beginFrame; i <= currentFrame; i++)
  {
    for (int pIdx = 0; pIdx < this->numPlayers; pIdx++)
    {
      const PlayerFrameDataQueue& inputQueue = pIdx == this->localPlayerIdx ?
                                                   this->localPlayerFrameData :
                                                   this->remotePlayerFrameData[pIdx];

      u32 frameDiff = i - this->rollbackInfo.beginFrame;          // idx.  [0, MAX_ROLLBACK_FRAMES]
      ASSERT(frameDiff < MAX_ROLLBACK_FRAMES && frameDiff >= 0);  // don't index out of bounds

      PlayerFrameData* inputToPopulate =
          &this->rollbackInfo.pastFrameDatas[frameDiff].playerFrameDatas[pIdx];

      // inputs we have received / are local should just be copied into past framedatas
      if (RANGE(i, this->rollbackInfo.beginFrame, confirmFrame) || pIdx == this->localPlayerIdx)
      {
        PlayerFrameData* pastInputs = findInPlayerFrameDataQueue(inputQueue, i);
        if (pastInputs)
        {
          INFO_LOG(BRAWLBACK, "Inserting inputs idx %u pidx %i frame %u\n", frameDiff, pIdx,
                   pastInputs->frame);
          memcpy(inputToPopulate, pastInputs, sizeof(PlayerFrameData));
        }
        else
        {
          ERROR_LOG(BRAWLBACK, "Couldn't find past inputs for rollback! Frame %u pidx %i\n", i,
                    pIdx);
        }
            else {
                ERROR_LOG(BRAWLBACK, "couldn't find remote input for rollback. frame %u\n", i);
      }
      // inputs we haven't received, and aren't local, and should still be predicted
      else
      {
        // remote inputs should be the predicted inputs
        PlayerFrameData predictedInputs =
            this->rollbackInfo.predictedInputs.playerFrameDatas[pIdx];
        predictedInputs.frame = i;  // rekey predicted inputs - pretending these are remote inputs
        predictedInputs.playerIdx = pIdx;
        INFO_LOG(BRAWLBACK, "Inserting predicted inputs idx %u pidx %i frame %u\n", frameDiff,
                 predictedInputs.playerIdx, predictedInputs.frame);
        *inputToPopulate = predictedInputs;
      }
    }

  }

  // this indicates that we should roll back on this frame
  this->rollbackInfo.pastFrameDataPopulated = true;

#if 0
    // print rollbackInfo
    INFO_LOG(BRAWLBACK, "RbInfo: beginFrame %u  endFrame: %u\n", rollbackInfo.beginFrame, confirmFrame);
    for (int i = 0; i < MAX_ROLLBACK_FRAMES; i++) {
        const FrameData& fd = rollbackInfo.pastFrameDatas[i];
        INFO_LOG(BRAWLBACK, "~~~~~~~ pastFramedatas[%i] ~~~~~~~\n", i);
        for (int pIdx = 0; pIdx < 2; pIdx++) {
            if (fd.playerFrameDatas[pIdx].frame != 0)
                INFO_LOG(BRAWLBACK, "pIdx %u:::   Frame %u\n", (unsigned int)fd.playerFrameDatas[pIdx].playerIdx, fd.playerFrameDatas[pIdx].frame);
        }
    }
#endif

#ifdef SYNCLOG
  for (int b = 0; b < MAX_ROLLBACK_FRAMES; b++)
  {
    if (rollbackInfo.pastFrameDatas[b].playerFrameDatas[0].frame <= confirmFrame)
      Sync::SyncLog(Sync::stringifyFramedata(rollbackInfo.pastFrameDatas[b], 2));
  }
#endif
}

u32 CEXIBrawlback::GetLatestRemoteFrame()
{
  u32 lowestFrame = 0;
  for (int i = 0; i < this->numPlayers; i++)
  {
    if (i == this->localPlayerIdx)
      continue;

    if (this->remotePlayerFrameData[i].empty())
    {
      return 0;
    }

    u32 f = this->remotePlayerFrameData[i].back()->frame;
    if (f < lowestFrame || lowestFrame == 0)
    {
      lowestFrame = f;
    }
  }

  return lowestFrame;
}

void BroadcastFramedataAck(u32 frame, u8 playerIdx, BrawlbackNetplay* netplay, ENetHost* server)
{
  FrameAck ackData;
  ackData.frame = (int)frame;
  ackData.playerIdx = playerIdx;
  sf::Packet ackDataPacket = sf::Packet();
  u8 cmdbyte = NetPacketCommand::CMD_FRAME_DATA_ACK;
  ackDataPacket.append(&cmdbyte, sizeof(cmdbyte));
  ackDataPacket.append(&ackData, sizeof(FrameAck));
  netplay->BroadcastPacket(ackDataPacket, ENET_PACKET_FLAG_UNSEQUENCED, server);
  // INFO_LOG(BRAWLBACK, "Sent ack for frame %u  pidx %u", frame, (unsigned int)playerIdx);
}

void CEXIBrawlback::ProcessIndividualRemoteFrameData(PlayerFrameData* framedata)
{
  u8 playerIdx = framedata->playerIdx;
  u32 frame = framedata->frame;
  PlayerFrameDataQueue& remoteFramedataQueue = this->remotePlayerFrameData[playerIdx];

  if (!remoteFramedataQueue.empty())
  {
    // if the remote frame we're trying to process is not newer than the most recent frame, we don't
    // care about it
    if (frame <= remoteFramedataQueue.back()->frame)
      return;
    // make sure the inputs we're adding are sequential
    ASSERT(frame == remoteFramedataQueue.back()->frame + 1);
  }

  std::unique_ptr<PlayerFrameData> f = std::make_unique<PlayerFrameData>(*framedata);
  INFO_LOG(BRAWLBACK, "Received opponent framedata. Player %u frame: %u (w/o delay %u)\n",
           (unsigned int)playerIdx, frame, frame - FRAME_DELAY);

  remoteFramedataQueue.push_back(std::move(f));
    if (!remoteFramedataQueue.empty()) {
        this->remotePlayerFrameDataMap[playerIdx][frame] = remoteFramedataQueue.back().get();
    }

  // clamp size of remote player framedata queue
  while (remoteFramedataQueue.size() > FRAMEDATA_MAX_QUEUE_SIZE)
  {
    // WARN_LOG(BRAWLBACK, "Hit remote player framedata queue max size! %u\n",
    // remoteFramedataQueue.size());
    remoteFramedataQueue.pop_front();
  }
}

void CEXIBrawlback::ProcessRemoteFrameData(PlayerFrameData* framedatas, u8 numFramedatas_u8)
{
  s32 numFramedatas = (s32)numFramedatas_u8;
  // framedatas may point to one or more PlayerFrameData's.
  // Also note. this array is the reverse of the local pad queue, in that
  // the 0th element here is the most recent framedata.
  PlayerFrameData* mostRecentFramedata = &framedatas[0];
  u32 frame = mostRecentFramedata->frame;
  u8 playerIdx = mostRecentFramedata->playerIdx;  // remote player idx

  // acknowledge that we received opponent's framedata
  BroadcastFramedataAck(frame, playerIdx, this->netplay.get(), this->server);
  // ---------------------

  // if (!this->remotePlayerFrameData[playerIdx].empty())
  // INFO_LOG(BRAWLBACK, "Received remote inputs. Head frame %u  received head frame %u\n",
  // this->remotePlayerFrameData[playerIdx].back()->frame, frame);

  if (numFramedatas > 0)
  {
    std::lock_guard<std::mutex> lock(remotePadQueueMutex);

    std::stringstream s;
    s << "Received " << numFramedatas << " framedatas. [";
    for (int i = 0; i < numFramedatas; i++)
    {
      s << framedatas[i].frame << ", ";
    }
    s << "]";
    INFO_LOG(BRAWLBACK, "%s\n", s.str().c_str());

    u32 maxFrame = 0;
    // index 0 is most recent, and we want to process new framedata oldest first, then newer ones
    for (s32 i = numFramedatas - 1; i >= 0; i--)
    {
      PlayerFrameData* framedata = &framedatas[i];
      maxFrame = framedata->frame > maxFrame ? framedata->frame : maxFrame;
      this->ProcessIndividualRemoteFrameData(framedata);
    }

    this->timeSync->ReceivedRemoteFramedata(maxFrame, localPlayerIdx, this->hasGameStarted);

  }

}

void CEXIBrawlback::ProcessFrameAck(FrameAck* frameAck)
{
  ASSERT(frameAck->playerIdx == this->localPlayerIdx);  // should be local player
  this->timeSync->ProcessFrameAck(frameAck);
}

void CEXIBrawlback::ProcessGameSettings(GameSettings* opponentGameSettings)
{
  // merge game settings for all remote/local players, then pass that back to the game

  this->localPlayerIdx = this->isHost ? 0 : 1;
  // assumes 1v1
  int remotePlayerIdx = this->isHost ? 1 : 0;

  //doing this so functionally equivalen to what White had before
  // Probably should just use gameSettings directly...
  GameSettings& mergedGameSettings = gameSettings;
  INFO_LOG(BRAWLBACK, "ProcessGameSettings for player %u\n", this->localPlayerIdx);
  INFO_LOG(BRAWLBACK, "Remote player idx: %i\n", remotePlayerIdx);

  mergedGameSettings.localPlayerIdx = this->localPlayerIdx;

  this->numPlayers = mergedGameSettings.numPlayers;
  INFO_LOG(BRAWLBACK, "Num players from emu: %u\n", (unsigned int)this->numPlayers);

  // this is kinda broken kinda unstable and weird.
  // hardcoded "fix" for testing. Get rid of this when you're confident this is stable
  if (this->numPlayers == 0)
  {
    this->numPlayers = 2;
    mergedGameSettings.numPlayers = 2;
  }

  if (!this->isHost)
  {  // is not host
    mergedGameSettings.randomSeed = opponentGameSettings->randomSeed;

    // get a random stage on the non-host side.
    mergedGameSettings.stageID = matchmaking->GetRandomStage();

    // if not host, your character will be p2, if host, your char will be p1

    // copy char into both slots
    mergedGameSettings.playerSettings[1].charID = mergedGameSettings.playerSettings[0].charID;
    // copy char from opponent p1 into our p1
    mergedGameSettings.playerSettings[0].charID = opponentGameSettings->playerSettings[0].charID;
  }
  else
  {  // is host
    // copy char from opponent's p2 into our p2
    mergedGameSettings.playerSettings[1].charID = opponentGameSettings->playerSettings[1].charID;

    // set our stage based on the one the other client generated
    mergedGameSettings.stageID = opponentGameSettings->stageID;
  }
  mergedGameSettings.playerSettings[localPlayerIdx].playerType =
      PlayerType::PLAYERTYPE_LOCAL;
  mergedGameSettings.playerSettings[remotePlayerIdx].playerType =
      PlayerType::PLAYERTYPE_REMOTE;

  // if we're not host, we just connected to host and received their game settings,
  // now we need to send our game settings back to them so they can start their game too
  if (!this->isHost)
  {
    this->netplay->BroadcastGameSettings(this->server, &mergedGameSettings);
  }

  std::lock_guard<std::mutex> lock(read_queue_mutex);
  this->read_queue.push_back(EXICommand::CMD_SETUP_PLAYERS);
  auto gameSettingsPtr = reinterpret_cast<u8*>(&mergedGameSettings);
  this->read_queue.insert(this->read_queue.end(), gameSettingsPtr,
                          gameSettingsPtr + sizeof(GameSettings));
}


// called from netplay thread
void CEXIBrawlback::ProcessNetReceive(ENetEvent* event)
{
  ENetPacket* pckt = event->packet;
  if (pckt && pckt->data && pckt->dataLength > 0)
  {
    u8* fullpckt_data = pckt->data;
    u8 cmd_byte = fullpckt_data[0];
    u8* data = &fullpckt_data[1];

    switch (cmd_byte)
    {
    case NetPacketCommand::CMD_FRAME_DATA:
    {
      u8 numFramedatas = data[0];
      PlayerFrameData* framedata = (PlayerFrameData*)(&data[1]);
      this->ProcessRemoteFrameData(framedata, numFramedatas);
    }
    break;
    case NetPacketCommand::CMD_FRAME_DATA_ACK:
    {
      FrameAck* frameAck = (FrameAck*)data;
      this->ProcessFrameAck(frameAck);
    }
    break;
    case NetPacketCommand::CMD_GAME_SETTINGS:
    {
      INFO_LOG(BRAWLBACK, "Received game settings from opponent");
      GameSettings* gameSettingsFromOpponent = (GameSettings*)data;
      this->ProcessGameSettings(gameSettingsFromOpponent);
    }
    break;
    default:
      WARN_LOG(BRAWLBACK, "Unknown packet cmd byte!");
      INFO_LOG(BRAWLBACK, "Packet as string: %s\n", fullpckt_data);
      break;
    }
  }
}

void CEXIBrawlback::NetplayThreadFunc()
{
  Common::SetCurrentThreadName("BrawlbackNetplay");
  ENetEvent event;

  // loop until we connect to someone, then after we connected,
  // do another loop for passing data between the connected clients

  INFO_LOG(BRAWLBACK, "Waiting for connection to opponent...");
  while (enet_host_service(this->server, &event, 0) >= 0 && !this->isConnected)
  {
    switch (event.type)
    {
    case ENET_EVENT_TYPE_CONNECT:
      INFO_LOG(BRAWLBACK, "Connected!");
      if (event.peer)
      {
        INFO_LOG(BRAWLBACK, "A new client connected from %x:%u\n", event.peer->address.host,
                 event.peer->address.port);
        this->isConnected = true;
      }
      else
      {
        WARN_LOG(BRAWLBACK, "Connect event received, but peer was null!");
      }
      break;
    case ENET_EVENT_TYPE_NONE:
      // INFO_LOG(BRAWLBACK, "Enet event type none. Nothing to do");
      break;
    }
  }

  if (this->isHost)
  {  // if we're host, send our game settings to clients right after connecting
    this->netplay->BroadcastGameSettings(this->server, &this->gameSettings);
  }

  INFO_LOG(BRAWLBACK, "Starting main net data loop");
  // main enet loop
  while (enet_host_service(this->server, &event, 0) >= 0 && this->isConnected &&
         !this->timeSync->getIsConnectionStalled())
  {
    this->netplay->FlushAsyncQueue(this->server);
    switch (event.type)
    {
    case ENET_EVENT_TYPE_DISCONNECT:
      // INFO_LOG(BRAWLBACK, "%s:%u disconnected.\n", event.peer -> address.host, event.peer ->
      // address.port);
      INFO_LOG(BRAWLBACK, "disconnected.\n");
      this->isConnected = false;
      break;
    case ENET_EVENT_TYPE_NONE:
      // INFO_LOG(BRAWLBACK, "Enet event type none. Nothing to do");
      break;
    case ENET_EVENT_TYPE_RECEIVE:
      this->ProcessNetReceive(&event);
      enet_packet_destroy(event.packet);
      break;
    }
  }

  ERROR_LOG(BRAWLBACK, "~~~~~~~~~~~~~ END ENET THREAD ~~~~~~~~~~~~~~~");
}

void CEXIBrawlback::MatchmakingThreadFunc()
{
  Common::SetCurrentThreadName("BrawlbackMatchmakingPhase2");
  while (this->matchmaking)
  {
    switch (this->matchmaking->GetMatchmakeState())
    {
    case Matchmaking::ProcessState::OPPONENT_CONNECTING:
      this->matchmaking->SetMatchmakeState(Matchmaking::ProcessState::CONNECTION_SUCCESS);
      this->connectToOpponent();
      break;
    case Matchmaking::ProcessState::ERROR_ENCOUNTERED:
      ERROR_LOG(BRAWLBACK, "MATCHMAKING: ERROR TRYING TO CONNECT!");
      return;
      break;
    default:
      break;
    }
  }
  INFO_LOG(BRAWLBACK, "~~~~~~~~~~~~~~ END MATCHMAKING THREAD ~~~~~~~~~~~~~~\n");
}

void CEXIBrawlback::connectToOpponent()
{
  this->isHost = this->matchmaking->IsHost();

  if (this->isHost)
  {
    INFO_LOG(BRAWLBACK, "Matchmaking: Creating server...");
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = this->matchmaking->GetLocalPort();

    this->server = enet_host_create(&address, 10, 3, 0, 0);
  }
  else
  {
    INFO_LOG(BRAWLBACK, "Matchmaking: Creating client...");
    this->server = enet_host_create(NULL, 10, 3, 0, 0);

    bool connectedToAtLeastOne = false;
    for (int i = 0; i < this->matchmaking->RemotePlayerCount(); i++)
    {
      ENetAddress addr;
      int set_host_res =
          enet_address_set_host(&addr, this->matchmaking->GetRemoteIPAddresses()[i].c_str());
      if (set_host_res < 0)
      {
        WARN_LOG(BRAWLBACK, "Failed to enet_address_set_host");
      }
      addr.port = this->matchmaking->GetRemotePorts()[i];

      ENetPeer* peer = enet_host_connect(this->server, &addr, 1, 0);
      if (peer == NULL)
      {
        WARN_LOG(BRAWLBACK, "Failed to enet_host_connect");
      }
      connectedToAtLeastOne = true;
    }
    if (!connectedToAtLeastOne)
    {
      ERROR_LOG(BRAWLBACK, "Failed to connect to any client/host");
      return;
    }
  }


  INFO_LOG(BRAWLBACK, "Net initialized, starting netplay thread");
  // loop to receive data over net
  this->netplay_thread = std::thread(&CEXIBrawlback::NetplayThreadFunc, this);
}

void CEXIBrawlback::handleFindMatch(u8* payload)
{
  // if (!payload) return;

#ifdef LOCAL_TESTING
  ENetAddress address;
  address.host = ENET_HOST_ANY;
  address.port = BRAWLBACK_PORT;

  this->server = enet_host_create(&address, 3, 0, 0, 0);

#define IP_FILENAME "/connect.txt"
  std::string connectIP = "127.0.0.1";

  if (File::Exists(File::GetExeDirectory() + IP_FILENAME))
  {
    std::fstream file;
    File::OpenFStream(file, File::GetExeDirectory() + IP_FILENAME, std::ios_base::in);
    connectIP.clear();
    std::getline(file, connectIP);  // read in only one line
    file.close();
    INFO_LOG(BRAWLBACK, "IP: %s\n", connectIP.c_str());
  }
  else
  {
    INFO_LOG(BRAWLBACK, "Creating connect file\n");
    std::fstream file;
    file.open(File::GetExeDirectory() + IP_FILENAME, std::ios_base::out);
    file << connectIP;
    file.close();
  }

  // just for testing. This should be replaced with a check to see if we are the "host" of the
  // match or not
  if (this->server == NULL)
  {
    this->isHost = false;
    WARN_LOG(BRAWLBACK, "Failed to init enet server!");
    WARN_LOG(BRAWLBACK, "Creating client instead...");
    this->server = enet_host_create(NULL, 3, 0, 0, 0);
    // for (int i = 0; i < 1; i++) { // make peers for all connecting opponents

    ENetAddress addr;
    int set_host_res = enet_address_set_host(&addr, connectIP.c_str());
    if (set_host_res < 0)
    {
      WARN_LOG(BRAWLBACK, "Failed to enet_address_set_host");
      return;
    }
    addr.port = BRAWLBACK_PORT;

    ENetPeer* peer = enet_host_connect(this->server, &addr, 1, 0);
    if (peer == NULL)
    {
      WARN_LOG(BRAWLBACK, "Failed to enet_host_connect");
      return;
    }

    //}
  }

  INFO_LOG(BRAWLBACK, "Net initialized, starting netplay thread");
  // loop to receive data over net
  this->netplay_thread = std::thread(&CEXIBrawlback::NetplayThreadFunc, this);
  return;
#endif

  Matchmaking::MatchSearchSettings search;
  std::string connectCode;

  // TODO: uncomment these lines when payload includes the actual mode and connect codes
#ifdef REMOVE_THIS_WHEN_PAYLOAD_IS_SET
  search.mode = (SlippiMatchmaking::OnlinePlayMode)payload[0];
  std::string shiftJisCode;
  shiftJisCode.insert(shiftJisCode.begin(), &payload[1], &payload[1] + 18);
  shiftJisCode.erase(std::find(shiftJisCode.begin(), shiftJisCode.end(), 0x00), shiftJisCode.end());
  connectCode = shiftJisCode;
#else
  search.mode = Matchmaking::OnlinePlayMode::UNRANKED;
#endif

  switch (search.mode)
  {
  case Matchmaking::OnlinePlayMode::DIRECT:
  case Matchmaking::OnlinePlayMode::TEAMS:
    search.connectCode = connectCode;
    break;
  default:
    break;
  }

  // Store this search so we know what was queued for
  lastSearch = search;
  matchmaking->FindMatch(search);
  this->matchmaking_thread = std::thread(&CEXIBrawlback::MatchmakingThreadFunc, this);
}

void CEXIBrawlback::handleStartMatch(u8* payload)
{
  // if (!payload) return;
  std::memcpy(&gameSettings, payload, sizeof(GameSettings));
}

void CEXIBrawlback::fixStartReplayEndianness(StartReplay& startReplay)
{
  startReplay.firstFrame = swap_endian(startReplay.firstFrame);
  startReplay.otherRandomSeed = swap_endian(startReplay.otherRandomSeed);
  startReplay.randomSeed = swap_endian(startReplay.randomSeed);
  for (auto& player : startReplay.players)
  {
    player.startPlayer.xPos = swap_endian(player.startPlayer.xPos);
    player.startPlayer.yPos = swap_endian(player.startPlayer.yPos);
    player.startPlayer.zPos = swap_endian(player.startPlayer.zPos);
  }
}

void CEXIBrawlback::serializeStartReplay(const StartReplay& startReplay)
{
  this->curReplayName = std::string(startReplay.nameBuffer, startReplay.nameBuffer + startReplay.nameSize);
  auto& start = this->curReplayJson["start"];
  for (int i = 0; i < startReplay.numPlayers; i++)
  {
    auto& player = start["players"][i];
    auto& position = player["startPlayerPos"];

    auto replayPlayer = startReplay.players[i];
    auto replayPosition = replayPlayer.startPlayer;

    player["ftKind"] = replayPlayer.fighterKind;
    player["slotID"] = replayPlayer.slotID;

    position["x"] = replayPosition.xPos;
    position["y"] = replayPosition.yPos;
    position["z"] = replayPosition.zPos;
  }
  start["stage"] = startReplay.stage;
  start["randomSeed"] = startReplay.randomSeed;
  start["otherRandomSeed"] = startReplay.otherRandomSeed;
  start["firstFrame"] = startReplay.firstFrame;
}


void CEXIBrawlback::handleStartReplaysStruct(u8* payload)
{
  if (SConfig::GetInstance().m_brawlbackSaveReplays)
  {
    StartReplay startReplay;
    std::memcpy(&startReplay, payload, sizeof(StartReplay));
    fixStartReplayEndianness(startReplay);
    serializeStartReplay(startReplay);
  }
}

void CEXIBrawlback::fixReplayEndianness(Replay& replay)
{
  replay.frameCounter = swap_endian(replay.frameCounter);
  replay.persistentFrameCounter = swap_endian(replay.persistentFrameCounter);
  for (auto& item : replay.items)
  {
    item.itemId = swap_endian(item.itemId);
    item.itemVariant = swap_endian(item.itemVariant);
  }
  for (auto& player : replay.players)
  {
    auto& input = player.inputs;
    auto& pos = player.pos;

    player.actionState = swap_endian(player.actionState);
    player.damage = swap_endian(player.damage);
    player.stockCount = swap_endian(player.stockCount);

    input.attack = swap_endian(input.attack);
    input.cStick = swap_endian(input.cStick);
    input.dTaunt = swap_endian(input.dTaunt);
    input.jump = swap_endian(input.jump);
    input.leftStickX = swap_endian(input.leftStickX);
    input.leftStickY = swap_endian(input.leftStickY);
    input.shield = swap_endian(input.shield);
    input.special = swap_endian(input.special);
    input.sTaunt = swap_endian(input.sTaunt);
    input.tapJump = swap_endian(input.tapJump);
    input.uTaunt = swap_endian(input.uTaunt);

    pos.xPos = swap_endian(pos.xPos);
    pos.yPos = swap_endian(pos.yPos);
    pos.zPos = swap_endian(pos.zPos);
  }
}

void CEXIBrawlback::serializeReplay(const Replay& replay)
{
  const auto frameName = fmt::format("frame_{}", replay.frameCounter);
  this->curReplayJson[frameName]["persistentFrameCounter"] = replay.persistentFrameCounter;
  for (int i = 0; i < replay.numItems; i++)
  {
    auto& item = this->curReplayJson[frameName]["items"][i];
    auto replayItem = replay.items[i];

    item["itemId"] = replayItem.itemId;
    item["itemVariant"] = replayItem.itemVariant;
  }
  for (int i = 0; i < replay.numPlayers; i++)
  {
    auto& player = this->curReplayJson[frameName]["players"][i];
    auto& inputs = player["inputs"];
    auto& position = player["position"];

    auto replayPlayer = replay.players[i];
    auto replayInputs = replayPlayer.inputs;
    auto replayPosition = replayPlayer.pos;

    player["actionState"] = replayPlayer.actionState;
    player["damage"] = replayPlayer.damage;
    player["stockCount"] = replayPlayer.stockCount;

    inputs["attack"] = replayInputs.attack;
    inputs["cStick"] = replayInputs.cStick;
    inputs["dTaunt"] = replayInputs.dTaunt;
    inputs["jump"] = replayInputs.jump;
    inputs["leftStickX"] = replayInputs.leftStickX;
    inputs["leftStickY"] = replayInputs.leftStickY;
    inputs["shield"] = replayInputs.shield;
    inputs["special"] = replayInputs.special;
    inputs["sTaunt"] = replayInputs.sTaunt;
    inputs["tapJump"] = replayInputs.tapJump;
    inputs["uTaunt"] = replayInputs.uTaunt;

    position["x"] = replayPosition.xPos;
    position["y"] = replayPosition.yPos;
    position["z"] = replayPosition.zPos;
  }
}
void CEXIBrawlback::handleReplaysStruct(u8* payload)
{
  if (SConfig::GetInstance().m_brawlbackSaveReplays)
  {
    Replay replay;
    std::memcpy(&replay, payload, sizeof(Replay));
    fixReplayEndianness(replay);
    serializeReplay(replay);
  }
}

void CEXIBrawlback::handleEndOfReplay()
{
  if (SConfig::GetInstance().m_brawlbackSaveReplays)
  {
    this->replayDirectory = SConfig::GetInstance().m_brawlbackReplayDir + "/" +
                            SConfig::GetInstance().m_details_game_id;
    auto ubjson = json::to_ubjson(this->curReplayJson);
    if (!std::filesystem::exists(replayDirectory))
    {
      std::filesystem::create_directory(replayDirectory);
    }
    writeToFile(replayDirectory + "/" +
                this->curReplayName + ".brba", ubjson.data(), ubjson.size());
  }
}

void CEXIBrawlback::handleGetStartReplay(u8* payload)
{
  std::memcpy(&this->curIndex, payload, sizeof(u8));
  StartReplay startReplay;
  auto indexReplay = this->getReplayJsonAtIndex(this->curIndex);
  auto name = this->getReplayNameAtIndex(this->curIndex);
  if (indexReplay == json({}) || name == std::string())
  {
    SendCmdToGame(CMD_BAD_INDEX);
    return;
  }
  auto start = indexReplay["start"];

  std::memcpy(startReplay.nameBuffer, name.data(), name.size());
  startReplay.nameSize = (u8)name.size();
  startReplay.firstFrame = start["firstFrame"];
  startReplay.stage = start["stage"];
  startReplay.randomSeed = start["randomSeed"];
  startReplay.otherRandomSeed = start["otherRandomSeed"];
  startReplay.numPlayers = (u8)start["players"].size();

  for (int i = 0; i < startReplay.numPlayers; i++)
  {
    auto player = start["players"][i];
    auto position = player["startPlayerPos"];

    auto& replayPlayer = startReplay.players[i];
    auto& replayPosition = replayPlayer.startPlayer;

    replayPlayer.fighterKind = player["ftKind"];
    replayPlayer.slotID = player["slotID"];

	replayPosition.xPos = position["x"];
    replayPosition.yPos = position["y"];
    replayPosition.zPos = position["z"];
  }
  SendCmdToGame(CMD_GET_START_REPLAY, &startReplay);
}

void CEXIBrawlback::handleGetNextFrame(u8* payload, int index)
{
  int frameNumber;
  std::memcpy(&frameNumber, payload, sizeof(int));
  frameNumber = swap_endian(frameNumber);

  auto indexJson = this->getReplayJsonAtIndex(index);
  if (indexJson == json({}))
  {
    SendCmdToGame(CMD_BAD_INDEX);
    return;
  }
  const auto frameName = fmt::format("frame_{}", frameNumber);

  auto frameJson = indexJson[frameName];
  Replay replay;

  replay.persistentFrameCounter = frameJson["persistentFrameCounter"];
  replay.frameCounter = frameNumber;
  replay.numItems = (u8)frameJson["items"].size();
  replay.numPlayers = (u8)frameJson["players"].size();

  for (int i = 0; i < replay.numItems; i++)
  {
    auto item = frameJson["items"][i];

	auto& replayItems = replay.items[i];

	replayItems.itemId = item["itemId"];
    replayItems.itemVariant = item["itemVarient"];
  }
  for (int i = 0; i < replay.numPlayers; i++)
  {
    auto replayPlayer = frameJson["players"][i];
    auto inputs = replayPlayer["inputs"];
    auto position = replayPlayer["position"];

	auto& replayPlayers = replay.players[i];
    auto& replayInputs = replayPlayers.inputs;
	auto& replayPos = replayPlayers.pos;

	replayPlayers.actionState = replayPlayer["actionState"];
    replayPlayers.damage = replayPlayer["damage"];
	replayPlayers.stockCount = replayPlayer["stockCount"];

    replayInputs.attack = inputs["attack"];
	replayInputs.cStick = inputs["cStick"];
    replayInputs.dTaunt = inputs["dTaunt"];
	replayInputs.jump = inputs["jump"];
    replayInputs.leftStickX = inputs["leftStickX"];
	replayInputs.leftStickY = inputs["leftStickY"];
    replayInputs.shield = inputs["shield"];
	replayInputs.special = inputs["special"];
    replayInputs.sTaunt = inputs["sTaunt"];
	replayInputs.tapJump = inputs["tapJump"];
    replayInputs.uTaunt = inputs["uTaunt"];

	replayPos.xPos = position["x"];
    replayPos.yPos = position["y"];
	replayPos.zPos = position["z"];
  }

  SendCmdToGame(CMD_GET_NEXT_FRAME, &replay);
}

void CEXIBrawlback::handleNumReplays()
{
  this->replayDirectory = SConfig::GetInstance().m_brawlbackReplayDir + "/" +
                          SConfig::GetInstance().m_details_game_id;
  auto numReplays = getNumReplays(replayDirectory);
  SendCmdToGame(CMD_GET_NUM_REPLAYS, &numReplays);
}

json CEXIBrawlback::getReplayJsonAtIndex(int index)
{
  auto replays = getReplays(replayDirectory);
  if (index > replays.size() - 1)
  {
    return json({});
  }
  return json::from_ubjson(replays[index]);
}

u8 CEXIBrawlback::getNumReplays(std::string path)
{
  u8 numReplayFiles = 0;
  for (auto& p : fs::directory_iterator(path))
  {
    if (p.path().extension().string() == ".brba")
    {
      numReplayFiles++;
    }
  }
  return numReplayFiles;
}

std::vector<std::vector<u8>> CEXIBrawlback::getReplays(std::string path)
{
  std::vector<std::vector<u8>> replays;
  for (auto& p : fs::directory_iterator(path))
  {
    if (p.path().extension().string() == ".brba")
    {
      std::vector<u8> replay = read_vector_from_disk(p.path().string());
      replays.push_back(replay);
    }
  }
  return replays;
}

std::string CEXIBrawlback::getReplayNameAtIndex(int index)
{
  auto names = getReplayNames(replayDirectory);
  if (index > names.size() - 1)
  {
    return std::string();
  }
  return names[index];
}

std::vector<std::string> CEXIBrawlback::getReplayNames(std::string path)
{
  std::vector<std::string> names;
  for (auto& p : fs::directory_iterator(path))
  {
    if (p.path().extension().string() == ".brba")
    {
      auto fullname = p.path().filename().string();
      size_t lastindex = fullname.find_last_of(".");
      names.push_back(fullname.substr(0, lastindex));
    }
  }
  return names;
}
    // recieve data from game into emulator
void CEXIBrawlback::DMAWrite(u32 address, u32 size)
{
  // INFO_LOG(BRAWLBACK, "DMAWrite size: %u\n", size);
  u8* mem = Memory::GetPointer(address);

  if (!mem)
  {
    INFO_LOG(BRAWLBACK, "Invalid address in DMAWrite!");
    // this->read_queue.clear();
    return;
  }

  u8 command_byte = mem[0];  // first byte is always cmd byte
  u8* payload = &mem[1];     // rest is payload

  // no payload
  if (size <= 1)
    payload = nullptr;

  static u64 frameTime = Common::Timer::GetTimeUs();
  switch (command_byte)
  {
  case CMD_UNKNOWN:
    INFO_LOG(BRAWLBACK, "Unknown DMAWrite command byte!");
    break;
  case CMD_ONLINE_INPUTS:
    // INFO_LOG(BRAWLBACK, "DMAWrite: CMD_ONLINE_INPUTS");
    handleLocalPadData(payload);
    break;
  case CMD_CAPTURE_SAVESTATE:
    // INFO_LOG(BRAWLBACK, "DMAWrite: CMD_CAPTURE_SAVESTATE");
    handleCaptureSavestate(payload);
    break;
  case CMD_LOAD_SAVESTATE:
    // INFO_LOG(BRAWLBACK, "DMAWrite: CMD_LOAD_SAVESTATE");
    handleLoadSavestate(payload);
    break;
  case CMD_FIND_OPPONENT:
    // INFO_LOG(BRAWLBACK, "DMAWrite: CMD_FIND_OPPONENT");
    handleFindMatch(payload);
    break;
  case CMD_START_MATCH:
    // INFO_LOG(BRAWLBACK, "DMAWrite: CMD_START_MATCH");
    handleStartMatch(payload);
    break;
  case CMD_REPLAY_START_REPLAYS_STRUCT:
    handleStartReplaysStruct(payload);
    break;
  case CMD_REPLAY_REPLAYS_STRUCT:
    handleReplaysStruct(payload);
    break;
  case CMD_REPLAYS_REPLAYS_END:
    handleEndOfReplay();
    break;
  case CMD_GET_NUM_REPLAYS:
    handleNumReplays();
	break;
  case CMD_GET_START_REPLAY:
    handleGetStartReplay(payload);
	break;
  case CMD_GET_NEXT_FRAME:
    handleGetNextFrame(payload, this->curIndex);
    break;

  // just using these CMD's to track frame times lol
  case CMD_TIMER_START:
  {
    frameTime = Common::Timer::GetTimeUs();
  }
  break;
  case CMD_TIMER_END:
  {
    u32 timeDiff = Common::Timer::GetTimeUs() - frameTime;
    INFO_LOG(BRAWLBACK, "Game logic took %f ms\n", (double)(timeDiff / 1000.0));
  }
  break;

  default:
    // INFO_LOG(BRAWLBACK, "Default DMAWrite %u\n", (unsigned int)command_byte);
    break;
  }

}

// send data from emulator to game
void CEXIBrawlback::DMARead(u32 address, u32 size)
{
  std::lock_guard<std::mutex> lock(read_queue_mutex);

  if (this->read_queue.empty())
  {                                                       // we have nothing to send to the game
    this->read_queue.push_back(EXICommand::CMD_UNKNOWN);  // result code
  }

  // game is trying to get cmd byte
  if (size == 1)
  {
    Memory::CopyToEmu(address, &this->read_queue[0], size);
    this->read_queue.erase(this->read_queue.begin());
    return;
  }

  this->read_queue.resize(size, 0);
  auto qAddr = &this->read_queue[0];
  Memory::CopyToEmu(address, qAddr, size);
  this->read_queue.clear();

}





// honestly dunno why these are overriden like this, but slippi does it sooooooo  lol
bool CEXIBrawlback::IsPresent() const
{
  return true;
}

void CEXIBrawlback::TransferByte(u8& byte)
{
}
