// TinySoundFontPlayer
// Very WIP, very untidy code. Needs some serious refactors. Needs to be formatted.
// Mostly works, if you ignore the things that don't.

// Haven't updated this list in a while. Will go back through this later.
// TODO save things like pitch bend, vol etc
// TODO less stanky pitch bend
// TODO pitch bend range
// TODO mod wheel modulation / pitch vibrato
// TODO smooth things like pitch bend etc
// TODO allow customising vib depth and rate, same for tremelo etc
// TODO fix the panning / stereo thing I had to remove
// TODO fix the filter thing I had to remove
// TODO think about what tsf_set_max_voices() should be
// TODO if using tsf_set_max_voices() find a way to kill previous voices to play new ones
// TODO figure out why the preset name stops showing up once you open and close the GUI
// TODO add option to force play notes outside of soundfont range (e.g. if soundfont only goes to C5 make it possible to play notes higher)
// TODO only apply gain if non-default to save CPU cycles

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define TSF_IMPLEMENTATION
#define TSF_CUBIC_INT
// TODO the only reason we have to use this is because the release envelope isn't updated fast enough if it's 0, see if we can add a special case for that?
#define TSF_RENDER_EFFECTSAMPLEBLOCK 1
#include "tsf.h"  // TinySoundFont header

#include "TinySoundFontPlayer.h"
#include "IPlug_include_in_plug_src.h"
#include "LFO.h"
#include <memory>
#include "IControls.h"

#include <memory>
#include <unordered_map>
#include <mutex>
#include <string>

// TODO Jank
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

class SoundFontCache {
public:
    // Either load and call tsf_copy or call tsf_copy on a cached tsf pointer
    // Ref counted, should unload tsf when ref count reaches 0
    // This lets multiple plugin instances share memory for large SoundFonts
    // E.g. user has multiple plugin instances which all load the file "SGM-V2.01.sf2" at the same location
    // TODO would it be worth hashing SF2 file instead of using path as cache entry? Seems like a very niche case.
    static std::shared_ptr<tsf> GetInstance(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& entry = cache_[path];
        if (!entry) {
            tsf* original = tsf_load_filename(path.c_str());
            if (!original) return nullptr;

            entry = std::make_shared<CacheEntry>();
            entry->tsfOriginal = original;
            entry->refCount = 0;
        }

        entry->refCount++;

        tsf* instanceCopy = tsf_copy(entry->tsfOriginal);
        if (!instanceCopy) {
            entry->refCount--;
            return nullptr;
        }

        // shared_ptr decrements ref count on cleanup
        return std::shared_ptr<tsf>(
            instanceCopy,
            [path](tsf* p) {
                CleanupInstance(path, p);
            }
        );
    }

private:
    struct CacheEntry {
        tsf* tsfOriginal = nullptr;
        size_t refCount = 0;
    };

    static void CleanupInstance(const std::string& path, tsf* instance) {
        // Copied instance should be closed because it is a seperate copy
        tsf_close(instance);

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            auto& entry = it->second;
            if (--entry->refCount == 0) {
                // Ref count hit 0, unload and remove from cache
                tsf_close(entry->tsfOriginal);
                cache_.erase(it);
            }
        }
    }

    static std::unordered_map<std::string, std::shared_ptr<CacheEntry>> cache_;
    static std::mutex mutex_;
};

std::unordered_map<std::string, std::shared_ptr<SoundFontCache::CacheEntry>> SoundFontCache::cache_;
std::mutex SoundFontCache::mutex_;

#define MAJOR_BIN_VER 1
#define MINOR_BIN_VER 1

// Taken from the tsf examples
//This is a minimal SoundFont with a single loopin saw-wave sample/instrument/preset (484 bytes)
const static unsigned char MinimalSoundFont[] =
{
#define TEN0 0,0,0,0,0,0,0,0,0,0
    'R','I','F','F',220,1,0,0,'s','f','b','k',
    'L','I','S','T',88,1,0,0,'p','d','t','a',
    'p','h','d','r',76,TEN0,TEN0,TEN0,TEN0,0,0,0,0,TEN0,0,0,0,0,0,0,0,255,0,255,0,1,TEN0,0,0,0,
    'p','b','a','g',8,0,0,0,0,0,0,0,1,0,0,0,'p','m','o','d',10,TEN0,0,0,0,'p','g','e','n',8,0,0,0,41,0,0,0,0,0,0,0,
    'i','n','s','t',44,TEN0,TEN0,0,0,0,0,0,0,0,0,TEN0,0,0,0,0,0,0,0,1,0,
    'i','b','a','g',8,0,0,0,0,0,0,0,2,0,0,0,'i','m','o','d',10,TEN0,0,0,0,
    'i','g','e','n',12,0,0,0,54,0,1,0,53,0,0,0,0,0,0,0,
    's','h','d','r',92,TEN0,TEN0,0,0,0,0,0,0,0,50,0,0,0,0,0,0,0,49,0,0,0,34,86,0,0,60,0,0,0,1,TEN0,TEN0,TEN0,TEN0,0,0,0,0,0,0,0,
    'L','I','S','T',112,0,0,0,'s','d','t','a','s','m','p','l',100,0,0,0,86,0,119,3,31,7,147,10,43,14,169,17,58,21,189,24,73,28,204,31,73,35,249,38,46,42,71,46,250,48,150,53,242,55,126,60,151,63,108,66,126,72,207,
    70,86,83,100,72,74,100,163,39,241,163,59,175,59,179,9,179,134,187,6,186,2,194,5,194,15,200,6,202,96,206,159,209,35,213,213,216,45,220,221,223,76,227,221,230,91,234,242,237,105,241,8,245,118,248,32,252
};

int TinySoundFontPlayer::FindPresetIndex(int bank, int presetNumber) const
{
    for (size_t i = 0; i < mPresets.size(); ++i)
    {
        if (mPresets[i].bank == bank && mPresets[i].preset_number == presetNumber)
            return static_cast<int>(i);
    }

    // Not found
    return -1;
}

// TODO This is really janky
void TinySoundFontPlayer::syncPresState() {

    mCurrentPresetIdx = FindPresetIndex(mCurrentPresetBank, mCurrentPresetNumber);
  if (mCurrentPresetIdx < 0) {
    mCurrentPresetIdx = 0;
  }

  mCurrentPresetIdxReal = mPresets[mCurrentPresetIdx].preset_index;

    if (GetUI()) {
        auto* pButton = GetUI()->GetControlWithTag(kCtrlTagPresetMenu)->As<IVButtonControl>();
        if (pButton && !mPresets.empty()) {
            pButton->SetValueStr(mPresets[mCurrentPresetIdx].name.Get());
        }
    }

  displayDirty = true;

}

// TODO This isn't very clean
bool TinySoundFontPlayer::SerializeState(IByteChunk& chunk) const
{
    const uint32_t majorSerialise = MAJOR_BIN_VER;
    const uint32_t minorSerialise = MINOR_BIN_VER;

    // Put version
    chunk.Put(&majorSerialise);
    chunk.Put(&minorSerialise);

    // Sub chunk

    IByteChunk subChunk;
    subChunk.PutStr(mSF2Path.Get());

    uint16_t presBank = mCurrentPresetBank;
    uint16_t presNum = mCurrentPresetNumber;

    subChunk.Put(&presBank);
    subChunk.Put(&presNum);

    const uint64_t chunkSize = subChunk.Size();

    // Put sub chunk
    chunk.Put(&chunkSize);
    chunk.PutChunk(&subChunk);

    // must remember to call SerializeParams at the end
    return SerializeParams(chunk); 
}

int TinySoundFontPlayer::UnserializeState(const IByteChunk& chunk, int startPos)
{
    uint32_t majorSerialise = 0;
    uint32_t minorSerialise = 0;
    uint64_t chunkSize = 0;
    startPos = chunk.Get(&majorSerialise, startPos);
    startPos = chunk.Get(&minorSerialise, startPos);
    startPos = chunk.Get(&chunkSize, startPos);

    uint64_t expectedSize = startPos + chunkSize;

    if (majorSerialise == MAJOR_BIN_VER) {

        if (minorSerialise != MINOR_BIN_VER) {
            // TODO alert of minor incompatibilities?
        }

        WDL_String loadedPath;
        startPos = chunk.GetStr(loadedPath, startPos);

        uint16_t presBank = 0;
        uint16_t presNum = 0;

        startPos = chunk.Get(&presBank, startPos);
        startPos = chunk.Get(&presNum, startPos);

        if (LoadNewTSFFile(loadedPath.Get()))
        {
            mSF2Path.Set(loadedPath.Get());
            sf2DisplayStr = loadedPath.get_filepart();
            displayDirty = true;

            mCurrentPresetBank = presBank;
            mCurrentPresetNumber = presNum;

            syncPresState();

            //tsf_channel_set_bank_preset(gTSFAtomic.load(std::memory_order_acquire), 0, mCurrentPresetBank, mCurrentPresetNumber);
          tsf_channel_set_bank(gTSFAtomic.load(std::memory_order_acquire), 0, mCurrentPresetBank);
          tsf_channel_set_presetnumber(gTSFAtomic.load(std::memory_order_acquire), 0, mCurrentPresetNumber, mCurrentPresetBank >= 127);
        }
        else
        {
            // File not found
            mSF2Path.Set("");
            // TODO probably show a popup or something? Or log it?
            LoadNewTSFMemory(MinimalSoundFont, sizeof(MinimalSoundFont));

            sf2DisplayStr = "Failed to load file " + std::string(loadedPath.get_filepart());
            displayDirty = true;
        }

        if (minorSerialise == MINOR_BIN_VER) {
            assert(expectedSize == startPos);
        }

        // must remember to call UnserializeParams at the end
        return UnserializeParams(chunk, expectedSize);
    } else {
        // TODO alert of major incompatibilities?

        mSF2Path.Set("");
        LoadNewTSFMemory(MinimalSoundFont, sizeof(MinimalSoundFont));

        return UnserializeParams(chunk, expectedSize);
    }
}

void TinySoundFontPlayer::clearPresStuff() {
    mPresets.clear();
    mCurrentPresetIdx = 0;
    mCurrentPresetIdxReal = 0;
    mCurrentPresetBank = 0;
    mCurrentPresetNumber = 0;
}

void TinySoundFontPlayer::loadDefaultPres() {
    tsf* f = gTSFAtomic.load(std::memory_order_acquire);

    if (!f) {
        return;
    }

    if (!mPresets.empty()) {
        PresetData pd = mPresets.front();
        mCurrentPresetIdx = 0;
        mCurrentPresetIdxReal = pd.preset_index;
        mCurrentPresetBank = pd.bank;
        mCurrentPresetNumber = pd.preset_number;

        displayDirty = true;

        //tsf_channel_set_bank_preset(gTSFAtomic.load(std::memory_order_acquire), 0, mCurrentPresetBank, mCurrentPresetNumber);
        tsf_channel_set_bank(f, 0, mCurrentPresetBank);
        tsf_channel_set_presetnumber(f, 0, mCurrentPresetNumber, mCurrentPresetBank >= 127);
    }
}

// TODO some vestigal code leftover from previous refactors
bool TinySoundFontPlayer::LoadNewTSFMemory(const void* data, size_t size)
{
    tsf* newTSF = tsf_load_memory(data, size);
    if (newTSF)
    {
        tsf_set_output(newTSF, TSF_STEREO_UNWEAVED, GetSampleRate(), 0);
#ifdef MAXVOICES
        // TODO consider making this changeable
        tsf_set_max_voices(newTSF, MAXVOICES);
#endif
        gTSFAtomic.store(newTSF, std::memory_order_release);

        PopulatePresetMenu();

        loadDefaultPres();
    }

    return newTSF;
}

// TODO some vestigal code leftover from previous refactors
bool TinySoundFontPlayer::LoadNewTSFFile(const char* filename)
{
  std::shared_ptr<tsf> shh = SoundFontCache::GetInstance(std::string(filename));

  //assert(shh);
  if (!shh) {
    return false;
  }

  tsf* newTSF = shh.get();

    if (newTSF)
    {
        tsf_set_output(newTSF, TSF_STEREO_UNWEAVED, GetSampleRate(), 0);
#ifdef MAXVOICES
        // TODO consider making this changeable
        tsf_set_max_voices(newTSF, MAXVOICES);
#endif
        gTSFAtomic.store(newTSF, std::memory_order_release);

        newTSFShared = shh;

        PopulatePresetMenu();

        loadDefaultPres();
    }

    return newTSF;
}


void TinySoundFontPlayer::PopulatePresetMenu()
{
    clearPresStuff();

    tsf* f = gTSFAtomic.load(std::memory_order_acquire);

    if (!f)
    {
        return;
    }

    int count = tsf_get_presetcount(f);

    for (int i = 0; i < count; ++i)
    {
        const char* name = tsf_get_presetname(f, i);
        int bank = 0;
        int preset = 0;

        // find bank/preset number
        for (int b = 0; b <= 127; ++b)
        {
            for (int p = 0; p <= 127; ++p)
            {
                int idx = tsf_get_presetindex(f, b, p);
                if (idx == i)
                {
                    bank = b;
                    preset = p;
                    goto found;
                }
            }
        }

    found:
        PresetData pd;
        pd.name.Set(name);
        pd.bank = bank;
        pd.preset_number = preset;
        pd.preset_index = i;
        mPresets.push_back(pd);
    }
}

TinySoundFontPlayer::TinySoundFontPlayer(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");

#if IPLUG_EDITOR
    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    };

    mLayoutFunc = [&](IGraphics* pGraphics) {
        pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
        pGraphics->AttachPanelBackground(COLOR_GRAY);
        pGraphics->EnableMouseOver(true);
        pGraphics->EnableMultiTouch(true);

#ifdef OS_WEB
        pGraphics->AttachPopupMenuControl();
#endif

        pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
        const IRECT b = pGraphics->GetBounds().GetPadded(-20.f);
        IRECT keyboardBounds = b.GetFromBottom(300);
        IRECT wheelsBounds = keyboardBounds.ReduceFromLeft(100.f).GetPadded(-10.f);
        pGraphics->AttachControl(new IVKeyboardControl(keyboardBounds), kCtrlTagKeyboard);
        pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5)), kCtrlTagBender);
        pGraphics->AttachControl(new IWheelControl(wheelsBounds.FracRectHorizontal(0.5, true), IMidiMsg::EControlChangeMsg::kModWheel));
        const IRECT controls = b.GetGridCell(1, 2, 2);
        pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(0, 2, 6).GetCentredInside(90), kParamGain, "Gain"));
#ifdef METERSENDER_NOBUG
        pGraphics->AttachControl(new IVLEDMeterControl<2>(controls.GetFromRight(100).GetPadded(-30)), kCtrlTagMeter);
#endif
        pGraphics->AttachControl(new IVButtonControl(keyboardBounds.GetFromTRHC(200, 30).GetTranslated(0, -30), SplashClickActionFunc,
                                                     "Show/Hide Keyboard", DEFAULT_STYLE.WithColor(kFG, COLOR_WHITE).WithLabelText({15.f, EVAlign::Middle})))->SetAnimationEndActionFunction(
                                                                                                                                                                                             [pGraphics](IControl* pCaller) {

                                                                                                                                                                                                 // TODO should this be static
                                                                                                                                                                                                 static bool hide = false;
                                                                                                                                                                                                 pGraphics->GetControlWithTag(kCtrlTagKeyboard)->Hide(hide = !hide);
                                                                                                                                                                                                 pGraphics->Resize(PLUG_WIDTH, hide ? PLUG_HEIGHT / 2 : PLUG_HEIGHT, pGraphics->GetDrawScale());
                                                                                                                                                                                             });

        pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg) {
            pGraphics->GetControlWithTag(kCtrlTagKeyboard)->As<IVKeyboardControl>()->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
        });

        const IRECT controls2 = b.GetGridCell(0, 2, 2);

        IRECT buttonBoundsTemp = controls2.GetGridCell(0, 2, 2).GetCentredInside(200, 25);
        IRECT buttonBounds = buttonBoundsTemp.GetFromBottom(35);
        IRECT buttonBoundsLower = buttonBoundsTemp.GetFromTop(35);
        IRECT buttonBoundsLowerLower = controls2.GetGridCell(1, 2, 2).GetCentredInside(200, 50);

        pGraphics->AttachControl(new IVLabelControl(buttonBoundsLower, sf2DisplayStr.c_str(), DEFAULT_STYLE.WithDrawFrame(false)), kCtrlTagSF2Name);

        auto loadFileFunc = [&](IControl* pCaller) {
            WDL_String fileName, path;

            pCaller->GetUI()->PromptForFile(fileName, path, EFileAction::Open, "sf2 sf3 SF2 SF3");

            if (path.GetLength())
            {
                DBGMSG("Selected path: %s\n", path.Get());
            }

            if (fileName.GetLength())
            {
                DBGMSG("Selected file: %s\n", fileName.Get());

                if (LoadNewTSFFile(fileName.Get())) {
                    mSF2Path.Set(fileName.Get());
                    sf2DisplayStr = fileName.get_filepart();
                    displayDirty = true;
                } else {
                    // alert of issue
                    mSF2Path.Set("");
                    LoadNewTSFMemory(MinimalSoundFont, sizeof(MinimalSoundFont));
                    sf2DisplayStr = "Failed to load file " + std::string(fileName.get_filepart());
                    displayDirty = true;
                }
            }
        };

        pGraphics->AttachControl(new IVButtonControl(buttonBoundsLowerLower, [this, pGraphics](IControl* pCaller) {
            if (!mPresets.empty()) {
              pCaller->As<IVButtonControl>()->SetValueStr(mPresets[mCurrentPresetIdx].name.Get());
            } else {
              pCaller->As<IVButtonControl>()->SetValueStr("(no presets in SoundFont)");
            }

            mMenu.SetFunction([pCaller, this](IPopupMenu* pMenu) {
                if (pMenu->GetChosenItem()) {
                    this->mCurrentPresetIdx = pMenu->GetChosenItemIdx();
                  PresetData* itemChosen = (PresetData*) pMenu->GetChosenItem();
                    this->mCurrentPresetIdxReal = itemChosen->preset_index;
                    pCaller->As<IVButtonControl>()->SetValueStr(mPresets[mCurrentPresetIdx].name.Get());

                    this->mCurrentPresetBank = mPresets[mCurrentPresetIdx].bank;
                    this->mCurrentPresetNumber = mPresets[mCurrentPresetIdx].preset_number;

                    //syncPresState();

                    this->displayDirty = true;

                    //tsf_channel_set_bank_preset(gTSFAtomic.load(std::memory_order_acquire), 0, mCurrentPresetBank, mCurrentPresetNumber);
                    tsf_channel_set_bank(gTSFAtomic.load(std::memory_order_acquire), 0, this->mCurrentPresetBank);
                    tsf_channel_set_presetnumber(gTSFAtomic.load(std::memory_order_acquire), 0, this->mCurrentPresetNumber, this->mCurrentPresetBank >= 127);

                }
            });

            mMenu.Clear();
            for (size_t i = 0; i < mPresets.size(); ++i) {
                mMenu.AddItem(mPresets[i].name.Get(), -1);
            }

            float x, y;
            pGraphics->GetMouseDownPoint(x, y);
            pGraphics->CreatePopupMenu(*pCaller, mMenu, x, y);
        }, "Preset", DEFAULT_STYLE.WithValueText(IText(16)), false, true), kCtrlTagPresetMenu);

        pGraphics->AttachControl(new IVButtonControl(buttonBounds, loadFileFunc, "Load File"));
    };
#endif

    // Initialize with default soundfont
    tsf* tsfPtr = gTSFAtomic.load(std::memory_order_acquire);

    if (!tsfPtr)
    {
        mSF2Path.Set("");
        LoadNewTSFMemory(MinimalSoundFont, sizeof(MinimalSoundFont));
        sf2DisplayStr = "No file loaded";
        displayDirty = true;
    } else {
        sf2DisplayStr = mSF2Path.get_filepart();
        displayDirty = true;
    }
}

#if IPLUG_DSP
void TinySoundFontPlayer::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
    mParamSmoother.ProcessBlock(mParamsToSmooth, mModulations.GetList(), nFrames);

    tsf* tsfPtr = gTSFAtomic.load(std::memory_order_acquire);

    if (!tsfPtr)
    {
        mMidiQueue.Clear();

        memset(outputs[0], 0, nFrames * sizeof(sample));
        memset(outputs[1], 0, nFrames * sizeof(sample));

        return;
    }

    int framePos = 0;

    while (framePos < nFrames)
    {
        int nextEventOffset = nFrames;

        if (!mMidiQueue.Empty())
        {
            const IMidiMsg& msg = mMidiQueue.Peek();
            nextEventOffset = std::min(msg.mOffset, nFrames);
        }

        // Only render if we have a positive number of frames to render
        int framesToRender = nextEventOffset - framePos;

        if (framesToRender > 0)
        {
#ifdef SAMPLE_TYPE_FLOAT
            tsf_render_float_separate(tsfPtr, &outputs[0][framePos], &outputs[1][framePos], framesToRender, 0);
#else
#error Double rendering not supported
#endif
            framePos += framesToRender;
        }

        if (!mMidiQueue.Empty())
        {
            const IMidiMsg& msg = mMidiQueue.Peek();

            if (msg.mOffset <= framePos)
            {
                //const int ch = msg.Channel();
                const int ch = 0;
                const int status = msg.StatusMsg();

                switch (status)
                {
                    case IMidiMsg::kNoteOn:
                        if (msg.Velocity() > 0)
                            tsf_channel_note_on(tsfPtr, ch, msg.NoteNumber(), msg.Velocity() / 127.f);
                        else
                            tsf_channel_note_off(tsfPtr, ch, msg.NoteNumber());
                        break;
                    case IMidiMsg::kNoteOff:
                        tsf_channel_note_off(tsfPtr, ch, msg.NoteNumber());
                        break;
                    case IMidiMsg::kControlChange:
                        tsf_channel_midi_control(tsfPtr, ch, msg.ControlChangeIdx(), msg.mData2);
                        break;
                    case IMidiMsg::kPitchWheel:
                    {
                        int iVal = (msg.mData2 << 7) + msg.mData1;
                        tsf_channel_set_pitchwheel(tsfPtr, ch, iVal);
                    }
                        break;
                    case IMidiMsg::kProgramChange:
                        mCurrentPresetNumber = msg.Program();
                        syncPresState();
                        //tsf_channel_set_presetnumber(tsfPtr, ch, msg.Program(), (ch == 9));
                        //tsf_channel_set_presetnumber(gTSFAtomic.load(std::memory_order_acquire), 0, mCurrentPresetIdxReal, mCurrentPresetBank >= 128);
                        tsf_channel_set_presetnumber(tsfPtr, ch, msg.Program(), mCurrentPresetBank >= 127 || (ch == 9));
                        break;
                }

                mMidiQueue.Remove(); // Advance the queue
            }
            else
            {
                // If next MIDI event is beyond current framePos, do nothing
            }
        }
    }

    mMidiQueue.Flush(nFrames);

  for(int s=0; s < nFrames;s++)
  {
    double smoothedGain = mModulations.GetList()[kModGainSmoother][s];
    outputs[0][s] *= (float)smoothedGain;
    outputs[1][s] *= (float)smoothedGain;
  }

#ifdef METERSENDER_NOBUG
    mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
#endif
}

void TinySoundFontPlayer::OnIdle()
{
#ifdef METERSENDER_NOBUG
    mMeterSender.TransmitData(*this);
#endif

    if (displayDirty) {
        IGraphics* pGraphics = GetUI();
        if (pGraphics) {
            IVLabelControl* pVControl = GetUI()->GetControlWithTag(kCtrlTagSF2Name)->As<IVLabelControl>();
            if (pVControl) {
                pVControl->SetStr(sf2DisplayStr.c_str());
                pVControl->SetDirty();
            }

          IVButtonControl* pButton = GetUI()->GetControlWithTag(kCtrlTagPresetMenu)->As<IVButtonControl>();
          if (pButton) {
            if (!mPresets.empty()) {
              pButton->SetValueStr(mPresets[mCurrentPresetIdx].name.Get());
            } else {
              pButton->SetValueStr("(no presets in SoundFont)");
            }
          }

            displayDirty = false;
        }
    }
}

void TinySoundFontPlayer::OnReset()
{
#ifdef METERSENDER_NOBUG
    mMeterSender.Reset(GetSampleRate());
#endif
    mMidiQueue.Clear();
    // TODO clear tsf voices

    tsf* tsfPtr = gTSFAtomic.load(std::memory_order_acquire);
    if (tsfPtr)
    {
        tsf_set_output(tsfPtr, TSF_STEREO_UNWEAVED, GetSampleRate(), 0);
#ifdef MAXVOICES
        // TODO consider making this changeable
        tsf_set_max_voices(tsfPtr, MAXVOICES);
#endif
    }

    int blockSize = GetBlockSize();

    mModulationsData.Resize(blockSize * kNumModulations);
    mModulations.Empty();

    for(int i = 0; i < kNumModulations; i++)
    {
      mModulations.Add(mModulationsData.Get() + (blockSize * i));
    }
}

void TinySoundFontPlayer::ProcessMidiMsg(const IMidiMsg& msg)
{
    mMidiQueue.Add(msg);
    // UI
    SendMidiMsg(msg);
}

void TinySoundFontPlayer::OnParamChange(int paramIdx)
{
  double value = GetParam(paramIdx)->Value();

  switch (paramIdx) {
    /*case kParamNoteGlideTime:
      mSynth.SetNoteGlideTime(value / 1000.);
      break;*/
    case kParamGain:
      mParamsToSmooth[kModGainSmoother] = (double) value / 100.;
      //tsf_channel_set_volume(); TODO?
      // ALSO TODO make MIDI volume change gain knob
      break;
    /*case kParamSustain:
      mParamsToSmooth[kModSustainSmoother] = (T) value / 100.;
      break;
    case kParamAttack:
    case kParamDecay:
    case kParamRelease:
    {
      EEnvStage stage = static_cast<EEnvStage>(EEnvStage::kAttack + (paramIdx - kParamAttack));
      mSynth.ForEachVoice([stage, value](SynthVoice& voice) {
        dynamic_cast<IPlugInstrumentDSP::Voice&>(voice).mAMPEnv.SetStageTime(stage, value);
      });
      break;
    }
    case kParamLFODepth:
      mLFO.SetScalar(value / 100.);
      break;
    case kParamLFORateTempo:
      mLFO.SetQNScalarFromDivision(static_cast<int>(value));
      break;
    case kParamLFORateHz:
      mLFO.SetFreqCPS(value);
      break;
    case kParamLFORateMode:
      mLFO.SetRateMode(value > 0.5);
      break;
    case kParamLFOShape:
      mLFO.SetShape(static_cast<int>(value));
      break;*/
    default:
      break;
  }
}

void TinySoundFontPlayer::OnParamChangeUI(int paramIdx, EParamSource source)
{
#if IPLUG_EDITOR
    if (auto pGraphics = GetUI())
    {
        /*
         if (paramIdx == kParam...)
         {
         // This method left intentionally blank
         }
         */
    }
#endif
}

bool TinySoundFontPlayer::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
    /*
     if(ctrlTag == kCtrlTagBender && msgTag == IWheelControl::kMessageTagSetPitchBendRange)
     {
     const int bendRange = *static_cast<const int*>(pData);
     // Deal with pitch bend range later
     }
     */

    return false;
}
#endif
