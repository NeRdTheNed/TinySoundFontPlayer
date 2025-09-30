#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "tsf.h"
#include "Smoothers.h"

// TODO Linking issue is preventing this from compiling on macOS, fix later
#ifndef __APPLE__
#define METERSENDER_NOBUG
#endif 

using namespace iplug;

const int kNumPresets = 1;

struct PresetData {
  WDL_String name;
  int bank;
  int preset_number;
  int preset_index;
};

enum EParams
{
  kParamGain = 0,
  //kParamNoteGlideTime,
  //kParamAttack,
  //kParamDecay,
  //kParamSustain,
  //kParamRelease,
  //kParamLFOShape,
  //kParamLFORateHz,
  //kParamLFORateTempo,
  //kParamLFORateMode,
  //kParamLFODepth,
  kNumParams
};

enum EModulations
{
  kModGainSmoother = 0,
  kNumModulations,
};

#if IPLUG_DSP
// will use EParams in TinySoundFontPlayer_DSP.h
//#include "TinySoundFontPlayer_DSP.h"
#endif

enum EControlTags
{
  kCtrlTagMeter = 0,
  kCtrlTagLFOVis,
  kCtrlTagScope,
  kCtrlTagRTText,
  kCtrlTagKeyboard,
  kCtrlTagBender,
  kCtrlTagFilePicker,
  kCtrlTagSF2Name,
  kCtrlTagPresetDropdown,
  kCtrlTagPresetMenu,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class TinySoundFontPlayer final : public Plugin
{
public:
  TinySoundFontPlayer(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
public:
  bool SerializeState(IByteChunk &chunk) const override;
  int UnserializeState(const IByteChunk &chunk, int startPos) override;
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  //TinySoundFontPlayerDSP<sample> mDSP {16};
#ifdef METERSENDER_NOBUG
  IPeakAvgSender<2> mMeterSender;
#endif
  //ISender<1> mLFOVisSender;
  IMidiQueue mMidiQueue;

  WDL_String mSF2Path;

  std::string sf2DisplayStr = "No file loaded";
    std::atomic<bool> displayDirty = false;

  std::atomic<tsf*> gTSFAtomic{ nullptr };
  std::shared_ptr<tsf> newTSFShared = nullptr;
  //using TSFDeleterType = void(*)(tsf*);
  //static void TSFDeleter(tsf* ptr) { if (ptr) tsf_close(ptr); };
  //std::unique_ptr<tsf, TSFDeleterType> gTSFOwner{ nullptr, TSFDeleter };
  //std::unique_ptr<tsf> gTSFOwner{ nullptr };

  std::vector<PresetData> mPresets = {};
  int mCurrentPresetIdx = 0;
  int mCurrentPresetIdxReal = 0;

  int mCurrentPresetBank = 0;
  int mCurrentPresetNumber = 0;

  IPopupMenu mMenu;

  bool LoadNewTSFMemory(const void* data, size_t size);
  bool LoadNewTSFFile(const char* filename);
  void PopulatePresetMenu();
  void clearPresStuff();
  void loadDefaultPres();
  int FindPresetIndex(int bank, int presetNumber) const;
  void syncPresState();

  // TODO probably make these float
  WDL_TypedBuf<double> mModulationsData; // Sample data for global modulations (e.g. smoothed sustain)
  WDL_PtrList<double> mModulations; // Ptrlist for global modulations
  LogParamSmooth<double, kNumModulations> mParamSmoother;
  double mParamsToSmooth[kNumModulations];
#endif
};
