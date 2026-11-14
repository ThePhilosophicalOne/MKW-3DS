#pragma once
#include <mkw/menu_focus.hpp>
#include <mkw/credits.hpp>
#include <mkw/unlock_page.hpp>
#include <mkw/unlocks.hpp>
#include <array>
#include <vector>
#include <cstdint>
namespace mkw {
// NoGhosts: ConfirmPage (retail page 0x52) as TimeAttackBackPage (0x70) opens
// it for a Time Trials course without ghosts. TimeAttackBack: that empty page,
// on screen only while the section fades out after its OK.
// Save and licence pages (Release 1, include/mkw/save_store.hpp), appended:
// SaveCreating: section 0x10, busy page 0x4F with BMG 0x811 while the new save
// file is written. SaveError: sections 0x11 (corrupt) / 0x13 (cannot save),
// page 0x03/0x04 + 0x53. LicenseCreateConfirm / LicenseMiiMissing: ConfirmPage
// 0x52 (BMG 0x836+0x835 / 0x898+0x89D). LicenseMiiConfirm: BMG 0x83F / 0x840.
// LicenseReady: LicenseDisplayPage 0x66 (0x867 / 0x86B). LicenseSettings:
// LicenseManagementPage 0x67. LicenseErase / LicenseErased: page 0x68.
enum class UiPage {Title,MainMenu,Mode,Character,Vehicle,Drift,Cup,Course,Confirm,Race,Pause,QuitConfirm,Results,Error,Strap,License,EngineClass,VSMode,BattleMode,BattleCup,BattleStage,Ghost,Intro,NoGhosts,TimeAttackBack,
 SaveCreating,SaveError,LicenseCreateConfirm,LicenseMiiMissing,LicenseMiiConfirm,LicenseReady,LicenseSettings,LicenseErase,LicenseErased,
 // RecordsPage 0xCE-0xD2 (Check Records): MenuState::recordsTab 0-4.
 Records,
 // Release 1 unlocks and credits (include/mkw/unlocks.hpp, credits.hpp), appended:
 // Unlock: page 0xCD of sections 0x90-0x93 (one per reward, MenuState::unlockPage).
 // CreditsDemo: page 0x3D over the ending_demo race (sections 0x39/0x3A).
 // CreditsMovie: pages 0x3D + 0x3E (sections 0x3B/0x3C). Congratulations: page 0x3F (0x3D/0x3E).
 Unlock,CreditsDemo,CreditsMovie,Congratulations};
// Licence and save actions main.cpp carries out (the save store, the 3DS Mii
// picker applet): SelectLicense, PickMii, CreateLicense, ChangeMii,
// EraseLicense, RecreateSave (corrupt page OK), StartWithoutSaving, ExitGame.
enum class UiAction {None,StartRace,RestartRace,ResumeRace,Exit,SelectLicense,PickMii,CreateLicense,ChangeMii,EraseLicense,RecreateSave,StartWithoutSaving,ExitGame};
enum class UiSaveError {Corrupt,CannotSave};
// One licence card (refreshLicenseControl, PAL 0x805EC4E8), filled by main.cpp
// from the save store. trophy: 0 gold, 1 silver, 2 bronze, 3 none shown.
// One Record_N row of a records page: its label and value messages (BMG), the
// value's integer parameters and nested text (a name or the rank glyph).
struct UiRecordRow {unsigned label=0,value=0;int ints[2]={};char nested[48]={};bool operator==(const UiRecordRow&)const=default;};
struct UiLicenseCard {
 bool occupied=false,miiMissing=false;
 char name[48]={};                    // UTF-8 Mii name
 std::array<bool,4> row{};            // class row shown (Mirror needs unlock bit 0x34)
 std::array<std::array<uint8_t,8>,4> trophy{};
 std::array<bool,8> cup{};            // cup icon (unlocked in a shown class) or its "?" cover
 bool operator==(const UiLicenseCard&)const=default;
};
enum class UiMode {GrandPrix,TimeTrials,VS,Battle};
// The retail pause pages' buttons, in list order (RaceMenuPage vf+0x6C/+0x70;
// ids index the variant table PAL 0x808DA7A8): Grand Prix (page 0x17), VS
// (0x18) and Battle (0x1A) have {Continue, Quit}; Time Trials (0x19) has
// {Continue, Restart, Change Course, Change Character, Quit}.
enum class UiPauseButton {Continue,Quit,Restart,ChangeCourse,ChangeCharacter};
inline unsigned uiPauseButtonCount(UiMode mode){return mode==UiMode::TimeTrials?5u:2u;}
inline UiPauseButton uiPauseButton(UiMode mode,unsigned slot){
 constexpr UiPauseButton TimeTrials[5]={UiPauseButton::Continue,UiPauseButton::Restart,UiPauseButton::ChangeCourse,UiPauseButton::ChangeCharacter,UiPauseButton::Quit};
 if(mode==UiMode::TimeTrials)return TimeTrials[slot<5?slot:4];
 return slot==0?UiPauseButton::Continue:UiPauseButton::Quit;
}
enum class UiEngineClass {CC50,CC100,CC150,Mirror};
struct UiMainMenuControl {unsigned group;float x,y;};
// Seven source child controls in four frames. Centers combine the original
// widescreen BRCTR frame transform and child translation (source canvas units).
inline constexpr std::array<UiMainMenuControl,7> UiMainMenuControls{{
 {0,-135.f,68.8f},{1,86.4f,68.8f},{1,135.f,68.8f},{1,183.6f,68.8f},
 {2,-172.8f,-93.2f},{2,-97.2f,-93.2f},{3,135.f,-93.2f}
}};
struct UiInput {bool up=false,down=false,left=false,right=false,accept=false,back=false,start=false,switchType=false;};
// MultiControlInputManager's direction hold: fn_1_DF4E4 (PAL 805ef898) counts
// the 60 Hz frames one direction set is held, MultiControlInputManager_calc
// (805f0e94) moves on frame 0 and on frames 15, 30, 45... Counted in display
// frames, not loop passes. reset() is the page activation (fn_1_DF5A4).
class UiDirectionRepeat {
public:
 // The directions held now; frames: 60 Hz frames since the previous step.
 UiInput step(bool up,bool down,bool left,bool right,unsigned frames);
 void reset(){mask_=0;held_=0;}
private:
 unsigned mask_=0,held_=0;
};
struct UiSelection {
 unsigned driver=0,vehicle=0,course=0;bool automatic=false;
 UiMode mode=UiMode::GrandPrix;UiEngineClass engineClass=UiEngineClass::CC150;
 unsigned license=0,battleStage=0;bool teams=false,coinRunners=false;
 // Time Trials ghost page: 0 Race This Ghost, 1 Watch the Replay, 2 Solo.
 unsigned ghostChoice=2;
 // The class page's START switch (GPClassSelect fn_1_32F814, PAL 0x8083FBC8): one
 // shared state; it affects only the classes whose karts-and-bikes reward is
 // unlocked (50cc bit 30, 100cc bit 31), and START does nothing without either.
 bool mixedEngineTypes=false;
 bool operator==(const UiSelection&)const=default;
};
struct UiCharacterInfo {const char* name;unsigned retailId,weight;};
// The first 24 entries are the cooked retail race roster. Mii identity models
// require the separate profile-rendering path before they can enter a race.
inline constexpr std::array<UiCharacterInfo,26> UiCharacters{{
 {"Mario",0,1},{"Luigi",7,1},{"Peach",16,1},{"Yoshi",10,1},
 {"Baby Mario",6,0},{"Baby Peach",1,0},{"Toad",8,0},{"Koopa Troopa",14,0},
 {"Wario",11,2},{"Waluigi",2,2},{"Donkey Kong",9,2},{"Bowser",3,2},
 {"Baby Daisy",4,0},{"Baby Luigi",12,0},{"Birdo",17,1},{"Daisy",15,1},
 {"Diddy Kong",18,1},{"Dry Bones",5,0},{"Dry Bowser",21,2},{"Funky Kong",22,2},
 {"King Boo",19,2},{"Rosalina",23,2},{"Toadette",13,0},{"Bowser Jr.",20,1},
 {"Mii Outfit A",24,1},{"Mii Outfit B",26,1}
}};
// Full retail roster in screen row order; race storage order is independent.
inline constexpr std::array<unsigned,26> UiCharacterDrivers{{
 4,13,5,12, 6,22,7,17, 0,1,2,15, 3,14,16,23, 8,9,10,11, 20,21,19,18, 24,25
}};
// Choices match GarageChoices (VehiclesPerDriver per driver) and the vehicle sprite keys.
inline constexpr const char* UiVehicleNames[3][12]={
 {"Standard Kart S","Booster Seat","Mini Beast","Standard Bike S","Bullet Bike","Bit Bike","Cheep Charger","Tiny Titan","Blue Falcon","Quacker","Magikruiser","Jet Bubble"},
 {"Standard Kart M","Classic Dragster","Wild Wing","Standard Bike M","Mach Bike","Sugarscoot","Super Blooper","Daytripper","Sprinter","Zip Zip","Sneakster","Dolphin Dasher"},
 {"Standard Kart L","Offroader","Flame Flyer","Standard Bike L","Flame Runner","Wario Bike","Piranha Prowler","Jetsetter","Honeycoupe","Shooting Star","Spear","Phantom"}
};
inline constexpr const char* UiBattleStageNames[]={"Block Plaza","Delfino Pier","Funky Stadium","Chain Chomp Wheel","Thwomp Desert","SNES Battle Course 4","GBA Battle Course 3","N64 Skyscraper","GCN Cookie Land","DS Twilight House"};
// Platform-independent navigation. Scene mutation is exclusively handled by main.
class MenuState {
public:
 UiPage page=UiPage::Strap;UiSelection selection{};
 // The fastest usable ghost per course in milliseconds (0: none), filled in
 // by the race runtime before the Time Trial ghost page.
 std::array<uint32_t,32> ghostTimes{};
 unsigned cursor=0,age=0,focusAge=0;bool unavailable=false;
 // Save and licence pages: the four cards, the page's error kind, and the
 // Mii picked for a new or changed licence (its name for BMG 0x83F/0x840).
 std::array<UiLicenseCard,4> licenses{};
 UiSaveError saveError=UiSaveError::Corrupt;
 char pickedName[48]={};
 bool changingMii=false,miiFromSettings=false,pickedGuest=false;
 // The page after the strap screen: the save pages first when the boot needs them.
 UiPage afterStrap=UiPage::Intro;
 // Records pages: the tab (General, Favorites, Friends, Nintendo WFC, Other)
 // and its rows, filled by main.cpp from the save (RecordsPage_onInit: 6/4/3/4/6 rows).
 unsigned recordsTab=0,recordRowCount=0;
 std::array<UiRecordRow,6> recordRows{};
 // Release 1 unlocks: the active licence's flags (main.cpp: unlock::flagsOf after
 // licence select and after each unlock page), title variants (any licence saw
 // the true ending), and the unlock page shown (unlock_page.hpp).
 UnlockWords unlocks{};
 bool titleVariants=false;
 unlock::UnlockPage unlockPage{};
 // Per licence card: a reward is pending (fn_1_DB3EC PAL 0x805EB7A0 then opens the unlock
 // section instead of the main menu). Filled by main.cpp (unlock::pendingOn).
 std::array<bool,4> unlockPendingAt{};
 // Credits presses for main.cpp's credits::Run (they change no page here): START on a
 // credits page (skip, when allowed), A on Congratulations. main.cpp clears them.
 bool creditsSkip=false,creditsDone=false;
 // Credits pages: filled by main.cpp from its credits::Run every frame.
 struct Credits {
  credits::Kind kind=credits::Kind::Normal;
  std::array<credits::Slot,3> slots{};   // center, top, bottom
  bool textIn=false;unsigned textAge=0;  // the six text controls' last In/Out and frames since
  int pane=-1,previousPane=-1;unsigned paneAge=0; // EndingMovie pane in view, the one before, frames since the change
 } credits{};
 // Frames since the menu section began: the section's MenuBG keeps running
 // across its pages (MenuTransition carries it over each page change).
 unsigned sectionAge=0;
 // Results is the retail VS end-of-race menu (VSRaceEnd): Next Race / Quit
 // Game, or only Next after the final race of a VS set (IsFinalRace).
 bool finalRace=false;
 // TitlePage_onInit (PAL 0x8063AB6C) with unlock flag 0x38 set, as every unlock
 // is here: entering the title section (Title, License, MainMenu) draws title
 // variant 0-4 again, never the last one (Util::Random nextU32(5); the last one
 // lives in the section context +0x4C8, -1 at boot). It selects the title
 // picture, TitleBack's blurred license/main-menu picture and the title voice.
 // One seed per boot: every copy draws as the real state will (prefetching).
 static uint32_t titleSeed();
 unsigned titleVariant=1;int lastTitleVariant=-1;uint32_t titleRandom=titleSeed();
 UiFocusState focus{};
 UiAction input(UiInput);
 void advance(unsigned ticks);
 void enter(UiPage);
 void raceStarted(){enter(UiPage::Race);}
 void finish(bool last=false){finalRace=last;enter(UiPage::Results);}
 // The race keeps running behind its end-of-race menu.
 bool simulates()const{return page==UiPage::Race||page==UiPage::Results;}
 // Logical Race is selected before the pause exit animation completes.
 // Its assets must survive every frame of that transition back to gameplay.
 bool retainsRaceAssets()const{return simulates()||page==UiPage::Pause||page==UiPage::QuitConfirm;}
 unsigned count()const;unsigned columns()const;
 unsigned characterDriver(unsigned slot)const;
 // The retail character grid (unlock::characterGrid): the UiCharacters index of a
 // slot, -1 for a "?" button; and the slot's CharacterSelect<rows>_<band> button.
 int characterAt(unsigned slot)const;
 unlock::CharacterGrid characterGrid()const{return unlock::characterGrid(unlocks);}
 // The machine of a slot (vehicle choice 0..11 of the driver, -1 = "?").
 int vehicleAt(unsigned slot)const;
 unsigned vehicleRows()const;
 bool kartsOnly()const;bool bikesOnly()const;
 // A Grand Prix cup needs its class's unlock; VS and Time Trials any class (fn_1_2D5A14).
 bool cupSelectable(unsigned cup)const;
 // Whether a slot can take the focus ("?" buttons and locked cups cannot).
 bool selectable(unsigned slot)const;
 unsigned vehicleChoice(unsigned slot)const;
 unsigned vehicleSlot(unsigned choice)const;
 unsigned engineClassMovie(unsigned slot)const;
 // The Time Trials page after the course: the ghost list, or the no-ghost
 // popup (TimeAttackBackPage_onReady, PAL 0x806393C8) when the list is empty.
 UiPage ghostPage()const{return ghostTimes[selection.course%ghostTimes.size()]?UiPage::Ghost:UiPage::NoGhosts;}
 // The race runtime starts races with all 24 retail drivers and all twelve
 // machines each. Other choices stay browsable, but accepting one reports
 // "Not available" on its own page rather than at the final confirmation.
 static bool raceMode(UiMode mode){return mode==UiMode::VS||mode==UiMode::GrandPrix||mode==UiMode::TimeTrials||mode==UiMode::Battle;}
 // Only the original 150cc physics runs (Kinoko's class rate 1.0).
 static bool raceEngineClass(UiEngineClass){return true;}
 static bool raceDriver(unsigned driver){return driver<24;}
 static bool raceVehicle(unsigned choice){return choice<12;}
 bool supportsRace()const;
};
// material: the NW4R default material, lerp(color0,color1,texture) times the
// corner colours (pane TEV C0/C1 applied live, e.g. a white glow silhouette).
struct UiQuad {unsigned sprite;float x,y,w,h,alpha=1;uint32_t tint=0xffffffff;bool bottom=false;float u0=0,v0=0,u1=1,v1=1;unsigned movie=~0u;std::array<uint32_t,4> corners{{0xffffffff,0xffffffff,0xffffffff,0xffffffff}};bool additive=false;unsigned vehicleIcon=~0u;uint32_t vehicleColor0=0,vehicleColor1=0xffffffff;bool material=false;uint32_t color0=0,color1=0xffffffff;};
// The UiCharacters index of a retail character id (Mii Outfit A/B = 24/26), -1 if none.
inline int uiCharacterIndex(unsigned retailId){
 const unsigned id=unlock::isMiiOutfitA(retailId)?24u:unlock::isMiiOutfitB(retailId)?26u:retailId;
 for(unsigned i=0;i<UiCharacters.size();++i)if(UiCharacters[i].retailId==id)return int(i);
 return -1;
}
// The vehicle choice (0..11, UiVehicleNames / GarageChoices order) of a retail vehicle id:
// karts w+3f -> f (f<3) or f+3; bikes 18+w+3f -> 3+f (f<3) or 6+f.
inline unsigned uiVehicleChoice(unsigned id){const unsigned f=(id<18?id:id-18)/3;return id<18?(f<3?f:f+3):(f<3?3+f:6+f);}
// UiQuad::movie of the credits movie (thp/ending, part 2): menu_renderer draws its own
// stream (MenuRenderer::openCreditsMovie), not a UiMovieRegions entry.
inline constexpr unsigned UiCreditsMovie=0xFFFFFFFEu;
struct UiRacer {unsigned driver=0,rank=1,lap=1;bool finished=false;float x=0,z=0;};
struct UiRaceView {std::array<UiRacer,12> racers{};unsigned count=0;unsigned milliseconds=0;};
// The same command list is consumed by Citro3D and the host layout preview.
struct UiTransitionView;
void composeMenu(const MenuState&,const UiRaceView&,std::vector<UiQuad>&);
// A Menu.bmg text of the save and licence pages (cooked; U+0001 = name), "" if absent.
const char* uiSaveMessage(unsigned id);
void composeMenu(const MenuState&,const UiRaceView&,std::vector<UiQuad>&,const UiTransitionView&);
std::vector<UiQuad> composeMenu(const MenuState&,const UiRaceView&);
}
