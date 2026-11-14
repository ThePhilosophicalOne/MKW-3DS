#include <mkw/menu_ui.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace mkw {
namespace {
// The title sections (0x3F-0x44): TitleBack on the license and main-menu pages
// reads the title page's variant, so it lasts until the section is left.
bool titleSection(UiPage p){return p==UiPage::Title||p==UiPage::License||p==UiPage::MainMenu;}

constexpr unsigned MainMenuLicenseSettings=7;
unsigned moveMainMenu(unsigned cursor,const UiInput& key){
 if(cursor==MainMenuLicenseSettings){
  if(!key.down)return cursor;
  cursor=1;                           // the top row's control under it (Multiplayer 2P)
  return cursor;
 }
 if(key.up&&cursor<4)return MainMenuLicenseSettings;
 const auto& origin=UiMainMenuControls[cursor];
 if(key.up||key.down){
  // There are two visual rows, with different child counts. Choose the
  // closest child in the other row using original control centers.
  unsigned best=cursor;float distance=1e9f;
  for(unsigned i=0;i<UiMainMenuControls.size();++i){
   const auto& target=UiMainMenuControls[i];
   if(target.y==origin.y)continue;
   const float delta=std::fabs(target.x-origin.x);
   if(delta<distance){distance=delta;best=i;}
  }
  return best;
 }
 const unsigned first=cursor<4?0:4,last=cursor<4?3:6;
 return key.left?(cursor==first?last:cursor-1):(cursor==last?first:cursor+1);
}

void normalize(UiSelection& s){
 if(s.driver>=UiCharacters.size())s.driver=0;
 if(s.vehicle>=12)s.vehicle=0;
 if(s.course>=32)s.course=0;
 if(s.battleStage>=10)s.battleStage=0;
 if(s.license>=4)s.license=0;
 if(unsigned(s.mode)>unsigned(UiMode::Battle))s.mode=UiMode::GrandPrix;
 if(unsigned(s.engineClass)>unsigned(UiEngineClass::Mirror))s.engineClass=UiEngineClass::CC150;
}

unsigned gridColumn(UiPage page,unsigned slot,unsigned columns){
 // The Mii buttons (CharacterSelect<rows>_3 Button0/1) sit below the three weight
 // bands in the last two columns (x 0/115 with three columns, 47/141 with four).
 return page==UiPage::Character&&slot>=columns*6?columns-2+(slot-columns*6):slot%columns;
}
// Buttons without a character or machine ("?", player flags 0) and locked cups
// (fn_1_2D54E0 PAL 0x807E5894) cannot take the focus.
template<class Selectable>
unsigned moveGrid(UiPage page,unsigned cursor,unsigned count,unsigned columns,const UiInput& key,Selectable selectable){
 if(count<=1)return 0;
 const bool vertical=key.up||key.down;
 const int direction=(key.up||(!vertical&&key.left))?-1:1;
 const unsigned rows=(count+columns-1)/columns;
 const unsigned column=gridColumn(page,cursor,columns),row=cursor/columns;
 const unsigned limit=vertical?rows:columns;
 // Search along one axis only. This preserves spatial navigation and skips the
 // two empty slots preceding the Mii buttons instead of selecting a phantom.
 for(unsigned step=1;step<=limit;++step){
  const unsigned target=(int(vertical?row:column)+int(limit)+direction*int(step))%limit;
  for(unsigned candidate=0;candidate<count;++candidate){
   const unsigned cr=candidate/columns,cc=gridColumn(page,candidate,columns);
   if((vertical?(cc==column&&cr==target):(cr==row&&cc==target))&&selectable(candidate))return candidate;
  }
 }
 return cursor;
}
}

unsigned MenuState::count()const{
 switch(page){
 case UiPage::License:case UiPage::Mode:return 4;
 // ClassSelect (3 buttons) until Mirror is unlocked, then ClassSelect2 (fn_1_32F0F4 PAL 0x8083F4A8).
 case UiPage::EngineClass:return unlock::engineClassButtons(unlocks);
 // YesNoButton Option0/Option1; LicenseManagementButton Record/ChangeMii/Delete.
 case UiPage::SaveError:case UiPage::LicenseMiiConfirm:case UiPage::LicenseErase:return 2;
 case UiPage::LicenseSettings:return 3;
 case UiPage::Records:return 1;
 case UiPage::Results:return finalRace?1:2;
 case UiPage::MainMenu:return unsigned(UiMainMenuControls.size())+1;
 case UiPage::Character:return characterGrid().count();
 case UiPage::Vehicle:return vehicleRows()*(kartsOnly()||bikesOnly()?1:2);
 case UiPage::VSMode:case UiPage::BattleMode:case UiPage::BattleCup:case UiPage::Drift:case UiPage::QuitConfirm:return 2;
 case UiPage::Cup:return 8;
 case UiPage::Course:return 4;
 case UiPage::BattleStage:return 5;
 case UiPage::Ghost:return 3;
 case UiPage::Pause:return uiPauseButtonCount(selection.mode);
 default:return 1;
 }
}

unsigned MenuState::columns()const{
 switch(page){
 case UiPage::License:case UiPage::MainMenu:return 2;
 case UiPage::Character:return characterGrid().rows;
 case UiPage::Cup:return 4;
 case UiPage::Vehicle:return kartsOnly()||bikesOnly()?1:2;
 case UiPage::BattleCup:return 2;
 default:return 1;
 }
}

// The visual grid is row-major: rows of grid.rows buttons, two rows per weight
// band (light, medium, heavy), then the Mii row. Button k of
// CharacterSelect<rows>_<band> at (column c, row r) is unlock::characterButton.
int MenuState::characterAt(unsigned slot)const{
 const auto grid=characterGrid();const unsigned w=grid.rows;
 int id=-1;
 if(slot<w*6){const unsigned v=slot/w,c=slot%w;id=grid.button[(v/2)*w*2+unlock::characterButton(c,v%2)];}
 else if(slot<grid.count())id=grid.mii[slot-w*6];
 return id<0?-1:uiCharacterIndex(unsigned(id));
}
unsigned MenuState::characterDriver(unsigned slot)const{
 const int i=characterAt(slot);return i<0?unsigned(UiCharacters.size()):unsigned(i);
}

bool MenuState::kartsOnly()const{return selection.mode==UiMode::GrandPrix&&selection.engineClass==UiEngineClass::CC50&&!(selection.mixedEngineTypes&&unlock::mixedTypesUnlocked(unlocks,0));}
bool MenuState::bikesOnly()const{return selection.mode==UiMode::GrandPrix&&selection.engineClass==UiEngineClass::CC100&&!(selection.mixedEngineTypes&&unlock::mixedTypesUnlocked(unlocks,1));}
unsigned MenuState::vehicleRows()const{return unlock::vehicleColumns(unlocks,UiCharacters[std::min(selection.driver,unsigned(UiCharacters.size()-1))].weight).rows;}
// VehicleSelectPage (fn_1_33515C PAL 0x80845510): a kart column and a bike column of
// the driver's weight class, each its unlocked machines in retail order; "?" below.
int MenuState::vehicleAt(unsigned slot)const{
 const unsigned weight=UiCharacters[std::min(selection.driver,unsigned(UiCharacters.size()-1))].weight;
 const auto columns=unlock::vehicleColumns(unlocks,weight);
 const unsigned n=kartsOnly()||bikesOnly()?1:2,row=slot/n;
 const unsigned kind=kartsOnly()?0:bikesOnly()?1:slot%n;
 if(row>=columns.rows||row>=6)return -1;
 const int id=columns.column[kind][row];
 return id<0?-1:int(uiVehicleChoice(unsigned(id)));
}
unsigned MenuState::vehicleChoice(unsigned slot)const{
 const int c=vehicleAt(slot);return c<0?12u:unsigned(c);
}

unsigned MenuState::vehicleSlot(unsigned choice)const{
 for(unsigned i=0;i<count();++i)if(vehicleAt(i)==int(choice))return i;
 for(unsigned i=0;i<count();++i)if(vehicleAt(i)>=0)return i;
 return 0;
}

unsigned MenuState::engineClassMovie(unsigned slot)const{
 slot=std::min(slot,3u);
 return selection.mixedEngineTypes&&slot<2&&unlock::mixedTypesUnlocked(unlocks,slot)?slot+4:slot;
}

bool MenuState::cupSelectable(unsigned cup)const{
 const bool mirror=selection.engineClass==UiEngineClass::Mirror;
 return unlock::cupOpen(unlocks,cup,selection.mode==UiMode::GrandPrix,mirror?2u:unsigned(selection.engineClass),mirror);
}
bool MenuState::selectable(unsigned slot)const{
 switch(page){
 case UiPage::Character:return characterAt(slot)>=0;
 case UiPage::Vehicle:return vehicleAt(slot)>=0;
 case UiPage::Cup:return cupSelectable(slot);
 default:return true;
 }
}

bool MenuState::supportsRace()const{
 // Browsing a retail menu does not silently change the chosen race rules or
 // substitute a different character/machine when that runtime is unavailable.
 const bool courseReady=selection.mode==UiMode::Battle?selection.battleStage<10&&!selection.coinRunners:selection.course<32;
 return raceMode(selection.mode)&&(selection.mode!=UiMode::GrandPrix||raceEngineClass(selection.engineClass))&&!selection.teams&&raceDriver(selection.driver)&&raceVehicle(selection.vehicle)&&courseReady;
}

UiInput UiDirectionRepeat::step(bool up,bool down,bool left,bool right,unsigned frames){
 // fn_1_DF4E4's mask: up 1 (else down 2), right 4 (else left 8). The count
 // restarts only from or to no direction; fn_1_DF6BC fires at 0 and 15+15k.
 const unsigned mask=(up?1u:down?2u:0u)|(right?4u:left?8u:0u);
 auto repeats=[](unsigned held){return held<15?0u:(held-15)/15+1;};
 const bool press=mask&&!mask_;const unsigned before=held_;
 held_=mask&&mask_?held_+frames:0;mask_=mask;
 UiInput key;
 if(mask&&(press||repeats(held_)>repeats(before))){key.up=mask&1;key.down=mask&2;key.right=mask&4;key.left=mask&8;}
 return key;
}

void MenuState::advance(unsigned ticks){age+=ticks;focusAge+=ticks;sectionAge+=ticks;uiFocusAdvance(*this,ticks);}

// Retail seeds Util::Random from the clock.
uint32_t MenuState::titleSeed(){static const uint32_t seed=uint32_t(std::time(nullptr))*2654435761u|1u;return seed;}

void MenuState::enter(UiPage next){
 normalize(selection);
 // Leaving the race (results, pause) is a section change: a new MenuBG.
 if(retainsRaceAssets())sectionAge=0;
 // TitlePage_onInit (PAL 0x8063AB6C..0x8063AD24): a new title variant, never the
 // last, once any licence has seen the true ending (unlock bit 0x38); else variant 0.
 if(titleSection(next)&&!titleSection(page)&&!titleVariants){titleVariant=0;lastTitleVariant=0;}
 else if(titleSection(next)&&!titleSection(page)){
  do{titleRandom^=titleRandom<<13;titleRandom^=titleRandom>>17;titleRandom^=titleRandom<<5;titleVariant=titleRandom%5;}while(int(titleVariant)==lastTitleVariant);
  lastTitleVariant=int(titleVariant);
 }
 page=next;age=focusAge=0;unavailable=false;cursor=0;
 switch(page){
 case UiPage::License:cursor=selection.license;break;
 case UiPage::Mode:cursor=unsigned(selection.mode);break;
 case UiPage::EngineClass:cursor=std::min(unsigned(selection.engineClass),count()-1);break;
 case UiPage::VSMode:cursor=selection.teams?1:0;break;
 case UiPage::BattleMode:cursor=selection.coinRunners?1:0;break;
 case UiPage::Character:
  for(unsigned i=0;i<count();++i)if(characterAt(i)==int(selection.driver)){cursor=i;break;}
  break;
 case UiPage::Vehicle:cursor=vehicleSlot(selection.vehicle);break;
 case UiPage::Drift:cursor=selection.automatic?0:1;break;
 case UiPage::Cup:cursor=selection.course/4;if(!cupSelectable(cursor))cursor=0;break;   // Mushroom is always open
 case UiPage::Course:cursor=selection.course%4;break;
 case UiPage::BattleCup:cursor=selection.battleStage/5;break;
 case UiPage::BattleStage:cursor=selection.battleStage%5;break;
 case UiPage::Ghost:cursor=ghostTimes[selection.course%ghostTimes.size()]?0:2;break; // Race This Ghost, or Solo when none
 default:break;
 }
 uiFocusReset(*this);
}

UiAction MenuState::input(UiInput key){
 normalize(selection);
 cursor=std::min(cursor,count()-1);
 if(page==UiPage::Race){if(key.start)enter(UiPage::Pause);return UiAction::None;}
 // RaceMenuPage_onInit (PAL 0x80858D60) gives the pause pages only an input
 // handler 2 (START), fn_1_349CE4 0x8085A098: close and resume. No page of
 // the family has a Back handler, so B does nothing on Pause or QuitConfirm.
 if(page==UiPage::Pause&&key.start){enter(UiPage::Race);return UiAction::ResumeRace;}
 if((page==UiPage::Pause||page==UiPage::QuitConfirm)&&key.back)return UiAction::None;
 if(key.back){
  switch(page){
  case UiPage::Intro:case UiPage::License:enter(UiPage::Title);break;
  case UiPage::MainMenu:enter(UiPage::License);break;
  // ConfirmPage's back (result 1) and the licence pages' Back buttons.
  case UiPage::LicenseCreateConfirm:case UiPage::LicenseMiiMissing:enter(UiPage::License);break;
  case UiPage::LicenseSettings:enter(UiPage::MainMenu);break;
  case UiPage::LicenseErase:enter(UiPage::LicenseSettings);break;
  // fn_1_34F12C (PAL 0x8085F4E0): Back from the first records page returns to
  // licence settings (page 0x67), from the others to the previous page.
  case UiPage::Records:if(recordsTab){--recordsTab;enter(UiPage::Records);}else enter(UiPage::LicenseSettings);break;
  case UiPage::Mode:enter(UiPage::MainMenu);break;
  case UiPage::EngineClass:case UiPage::VSMode:case UiPage::BattleMode:enter(UiPage::Mode);break;
  case UiPage::Character:
   enter(selection.mode==UiMode::GrandPrix?UiPage::EngineClass:selection.mode==UiMode::VS?UiPage::VSMode:selection.mode==UiMode::Battle?UiPage::BattleMode:UiPage::Mode);break;
  case UiPage::Vehicle:enter(UiPage::Character);break;
  case UiPage::Drift:enter(UiPage::Vehicle);break;
  case UiPage::Cup:case UiPage::BattleCup:enter(UiPage::Drift);break;
  case UiPage::Course:enter(UiPage::Cup);break;
  case UiPage::BattleStage:enter(UiPage::BattleCup);break;
  case UiPage::Ghost:enter(UiPage::Course);break;
  // ConfirmPage's back (PAL 0x805C22B8, result 1): TimeAttackBackPage_onRefocus
  // (0x80639260) replaces itself with page 0x6F, RaceCourseSelectPage.
  case UiPage::NoGhosts:enter(UiPage::Course);break;
  case UiPage::Confirm:case UiPage::Error:
   enter(selection.mode==UiMode::GrandPrix?UiPage::Cup:selection.mode==UiMode::Battle?UiPage::BattleStage:selection.mode==UiMode::TimeTrials?ghostPage():UiPage::Course);break;
  default:break;
  }
  return UiAction::None;
 }
 if(key.switchType&&page==UiPage::EngineClass){
  // GPClassSelect::OnStartPress (USA 80825168, PAL fn_1_32F814 0x8083FBC8) uses one
  // toggle for both classes; it only acts once 50cc or 100cc karts-and-bikes is
  // unlocked (bits 30/31), and each class follows its own bit (kartsOnly/bikesOnly).
  if(!unlock::anyMixedTypesUnlocked(unlocks))return UiAction::None;
  selection.mixedEngineTypes=!selection.mixedEngineTypes;
  focusAge=0;unavailable=false;
  return UiAction::None;
 }
 if(key.up||key.down||key.left||key.right){
  const unsigned previousCursor=cursor;
  if(page==UiPage::MainMenu)cursor=moveMainMenu(cursor,key);
  else if(columns()==1){
   // Horizontal input also traverses a one-column control group, as it does
   // when navigating the same controls with the Wii controller sideways.
   const int step=key.up||(!key.down&&key.left)?-1:1;
   cursor=(int(cursor)+int(count())+step)%count();
  }else cursor=moveGrid(page,cursor,count(),columns(),key,[&](unsigned i){return selectable(i);});
  if(cursor!=previousCursor)focusAge=0;
  uiFocusSelect(*this,previousCursor);unavailable=false;
 }
 // Credits pages 0x3D: input handler 2 (START), installed only once that ending was
 // seen (fn_1_BAE10 PAL 0x805CB1C4); main.cpp asks credits::Run whether it applies.
 if(key.start&&(page==UiPage::CreditsDemo||page==UiPage::CreditsMovie)){creditsSkip=true;return UiAction::None;}
 if(!key.accept&&!(key.start&&(page==UiPage::Title||page==UiPage::Strap||page==UiPage::Intro)))return UiAction::None;
 switch(page){
 case UiPage::Strap:{const UiPage next=afterStrap;afterStrap=UiPage::Intro;enter(next);break;}
 case UiPage::Intro:enter(UiPage::Title);break;
 case UiPage::Title:enter(UiPage::License);break;
 // fn_1_DB3EC (PAL 0x805EB7A0): an empty card asks to create a licence
 // (ConfirmPage 0x836/0x835 -> section 0x45); a card whose Mii is missing asks
 // for another Mii (0x898/0x89D -> section 0x46); otherwise the licence is
 // selected (SaveManager_selectLicense) and the main menu opens.
 case UiPage::License:
  selection.license=cursor;
  if(!licenses[cursor].occupied)enter(UiPage::LicenseCreateConfirm);
  else if(licenses[cursor].miiMissing)enter(UiPage::LicenseMiiMissing);
  else{enter(unlockPendingAt[cursor]?UiPage::Unlock:UiPage::MainMenu);return UiAction::SelectLicense;}
  break;
 case UiPage::MainMenu:
  if(cursor==0)enter(UiPage::Mode);
  else if(cursor==MainMenuLicenseSettings)enter(UiPage::LicenseSettings);
  else unavailable=true;
  break;
 // The corrupt page's OK creates a new file; "Start without saving" turns
 // saving off; the second button leaves the game (retail: the Wii Menu).
 case UiPage::SaveError:
  if(cursor==1)return UiAction::ExitGame;
  return saveError==UiSaveError::Corrupt?UiAction::RecreateSave:UiAction::StartWithoutSaving;
 case UiPage::LicenseCreateConfirm:changingMii=false;miiFromSettings=false;return UiAction::PickMii;
 case UiPage::LicenseMiiMissing:changingMii=true;miiFromSettings=false;return UiAction::PickMii;
 // Yes writes the licence (LicenseDisplayPage_onActivate, PAL 0x805EC274);
 // No returns to the Mii choice.
 case UiPage::LicenseMiiConfirm:
  if(cursor==1)return UiAction::PickMii;
  return changingMii?UiAction::ChangeMii:UiAction::CreateLicense;
 // "Your new license is ready." returns to licence select (section 0x42).
 case UiPage::LicenseReady:enter(miiFromSettings?UiPage::LicenseSettings:UiPage::License);break;
 // LicenseManagementPage (0x67): Check Records needs the Records pages (a
 // later release); Change Mii (section 0x46); Erase License (page 0x68).
 case UiPage::LicenseSettings:
  if(cursor==0){recordsTab=0;enter(UiPage::Records);}
  else if(cursor==1){changingMii=true;miiFromSettings=true;return UiAction::PickMii;}
  else enter(UiPage::LicenseErase);
  break;
 case UiPage::LicenseErase:
  if(cursor==1){enter(UiPage::LicenseSettings);break;}
  return UiAction::EraseLicense;
 // LicenseDeletePage then returns to the title (fn_1_DE228 -> section 0x3F).
 case UiPage::LicenseErased:enter(UiPage::Title);break;
 // NextRecordButton: the next records page; Close on the last (0xD2) returns
 // to licence settings (section 0x47).
 case UiPage::Records:if(recordsTab<4){++recordsTab;enter(UiPage::Records);}else enter(UiPage::LicenseSettings);break;
 case UiPage::Mode:
  if(!raceMode(UiMode(cursor))){unavailable=true;break;}
  selection.mode=UiMode(cursor);
  enter(selection.mode==UiMode::GrandPrix?UiPage::EngineClass:selection.mode==UiMode::VS?UiPage::VSMode:selection.mode==UiMode::Battle?UiPage::BattleMode:UiPage::Character);break;
 case UiPage::EngineClass:
  if(!raceEngineClass(UiEngineClass(cursor))){unavailable=true;break;}
  selection.engineClass=UiEngineClass(cursor);enter(UiPage::Character);break;
 case UiPage::VSMode:
  if(cursor==1){unavailable=true;break;} // Team VS has no race runtime.
  selection.teams=false;enter(UiPage::Character);break;
 case UiPage::BattleMode:
  if(cursor==1){unavailable=true;break;} // Coin Runners needs coin collection and round scoring.
  selection.coinRunners=false;enter(UiPage::Character);break;
 case UiPage::Character:
  if(characterAt(cursor)<0)break;   // a "?" button (player flags 0)
  // Includes the two Mii outfits, which also need Mii data.
  if(!raceDriver(characterDriver(cursor))){unavailable=true;break;}
  selection.driver=characterDriver(cursor);enter(UiPage::Vehicle);break;
 case UiPage::Vehicle:
  if(vehicleAt(cursor)<0)break;
  if(!raceVehicle(vehicleChoice(cursor))){unavailable=true;break;}
  selection.vehicle=vehicleChoice(cursor);enter(UiPage::Drift);break;
 case UiPage::Drift:selection.automatic=cursor==0;enter(selection.mode==UiMode::Battle?UiPage::BattleCup:UiPage::Cup);break;
 case UiPage::Cup:
  if(!cupSelectable(cursor))break;
  // A Grand Prix selects the whole cup. VS and Time Trials select a course.
  if(selection.course/4!=cursor||selection.mode==UiMode::GrandPrix)selection.course=cursor*4;
  enter(selection.mode==UiMode::GrandPrix?UiPage::Confirm:UiPage::Course);break;
 case UiPage::Course:selection.course=selection.course/4*4+cursor;enter(selection.mode==UiMode::TimeTrials?ghostPage():UiPage::Confirm);break;
 // ConfirmPage's OK (PAL 0x805C21C0, result 0): TimeAttackBackPage_onRefocus
 // (0x80639260) sets up a solo run and changes section at once; there is no
 // race confirmation page on this path.
 case UiPage::NoGhosts:
  selection.ghostChoice=2;
  if(supportsRace())return UiAction::StartRace;
  unavailable=true;break;
 case UiPage::Ghost:
  // Racing or watching a ghost needs one for the course.
  if(cursor==2||ghostTimes[selection.course%ghostTimes.size()]){selection.ghostChoice=cursor;enter(UiPage::Confirm);}
  else unavailable=true;
  break;
 case UiPage::BattleCup:
  if(selection.battleStage/5!=cursor)selection.battleStage=cursor*5;
  enter(UiPage::BattleStage);break;
 case UiPage::BattleStage:selection.battleStage=selection.battleStage/5*5+cursor;enter(UiPage::Confirm);break;
 case UiPage::Confirm:if(supportsRace())return UiAction::StartRace;unavailable=true;break;
 // RaceMenuPage_onButtonFront (PAL 0x8085A0F4), by button id.
 case UiPage::Pause:
  switch(uiPauseButton(selection.mode,cursor)){
  case UiPauseButton::Continue:enter(UiPage::Race);return UiAction::ResumeRace; // case 0
  case UiPauseButton::Restart:return UiAction::RestartRace;                     // case 2
  // Cases 5 and 6 change section to 0x4A / 0x49, the single-player menus
  // entered at page 0x6E (cup select) / 0x6B (character select).
  case UiPauseButton::ChangeCourse:enter(UiPage::Cup);break;
  case UiPauseButton::ChangeCharacter:enter(UiPage::Character);break;
  // Case 1: with the section's confirmation page (0x2C, only Grand Prix
  // section 0x1E among the single-player races) it pushes that page,
  // otherwise it quits at once (section 0x41).
  case UiPauseButton::Quit:enter(selection.mode==UiMode::GrandPrix?UiPage::QuitConfirm:UiPage::MainMenu);break;
  }
  break;
 // Page 0x2C: ButtonConfirmContinue (case 0x17) closes the pages and resumes
 // while paused; ButtonConfirmQuit (case 0x18) quits as case 1 does.
 case UiPage::QuitConfirm:
  if(cursor==0){enter(UiPage::Race);return UiAction::ResumeRace;}
  enter(UiPage::MainMenu);break;
 case UiPage::Results:
  // RaceMenu::OnButtonClick (USA 0x80838664): Next goes to the VS course
  // choice (section 0x4B); Quit (and, without the award scene, the final
  // race's Next) returns to the menus (section 0x41).
  // A Grand Prix goes straight on to the cup's next course.
  if(cursor==0&&!finalRace&&selection.mode==UiMode::GrandPrix){selection.course=selection.course/4*4+std::min(selection.course%4+1,3u);enter(UiPage::Confirm);return UiAction::StartRace;}
  if(cursor==0&&!finalRace)enter(selection.mode==UiMode::Battle?UiPage::BattleCup:UiPage::Cup);
  else enter(UiPage::MainMenu);
  break;
 case UiPage::Error:enter(selection.mode==UiMode::GrandPrix?UiPage::Cup:selection.mode==UiMode::Battle?UiPage::BattleStage:selection.mode==UiMode::TimeTrials?ghostPage():UiPage::Course);break;
 // The unlock page's NextButton (fn_1_3440C8 PAL 0x8085447C): accepted once (the voice
 // is MenuSound's); B and START do nothing (fn_1_34439C, MenuPage vtable +0x7C). main.cpp
 // starts the section change at once; the transition holds the page nextDelayFrames.
 case UiPage::Unlock:if(!unlockPage.leaving())unlockPage.next();break;
 // Congratulations (page 0x3F): A (handler 0, fn_1_BCDF8).
 case UiPage::Congratulations:creditsDone=true;break;
 default:break;
 }
 return UiAction::None;
}
}
