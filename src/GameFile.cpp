#include "GameFile.h"

#include "Building.h"
#include "CityView.h"
#include "Empire.h"
#include "Event.h"
#include "FileSystem.h"
#include "Loader.h"
#include "SidebarMenu.h"
#include "Sound.h"
#include "Routing.h"
#include "Walker.h"
#include "Zip.h"

#include "Data/Grid.h"
#include "Data/Random.h"
#include "Data/Scenario.h"
#include "Data/Settings.h"
#include "Data/Message.h"
#include "Data/Empire.h"
#include "Data/CityInfo.h"
#include "Data/Walker.h"
#include "Data/Tutorial.h"
#include "Data/Sound.h"
#include "Data/Building.h"
#include "Data/Invasion.h"
#include "Data/Trade.h"
#include "Data/Walker.h"
#include "Data/Formation.h"
#include "Data/Debug.h"
#include "Data/Event.h"
#include "Data/State.h"

#include <stdio.h>
#include <string.h>

#define SCENARIO_PARTS 12
#define SAVEGAME_PARTS 300
#define COMPRESS_BUFFER_SIZE 600000
#define UNCOMPRESSED 0x80000000

struct GameFilePart {
	int compressed;
	void *data;
	int lengthInBytes;
};

static const int savegameVersion = 0x66;

static int savegameFileVersion;
static char playerNames[2][32];

static char compressBuffer[COMPRESS_BUFFER_SIZE]; // TODO use global malloc'ed scratchpad buffer

static char tmp[COMPRESS_BUFFER_SIZE]; // TODO remove when all savegame fields are known

static int endMarker = 0;

static const char missionPackFile[] = "mission1.pak";

static const char missionSavedGames[][32] = {
	"Citizen.sav",
	"Clerk.sav",
	"Engineer.sav",
	"Architect.sav",
	"Quaestor.sav",
	"Procurator.sav",
	"Aedile.sav",
	"Praetor.sav",
	"Consul.sav",
	"Proconsul.sav"
	"Caesar.sav",
	"Caesar2.sav"
};

static GameFilePart scenarioParts[SCENARIO_PARTS] = {
	{0, &Data_Grid_graphicIds, 52488},
	{0, &Data_Grid_edge, 26244},
	{0, &Data_Grid_terrain, 52488},
	{0, &Data_Grid_bitfields, 26244},
	{0, &Data_Grid_random, 26244},
	{0, &Data_Grid_elevation, 26244},
	{0, &Data_Random.iv1, 4},
	{0, &Data_Random.iv2, 4},
	{0, &Data_Settings_Map.camera.x, 4},
	{0, &Data_Settings_Map.camera.y, 4},
	{0, &Data_Scenario, 1720},
};

static GameFilePart saveGameParts[SAVEGAME_PARTS] = {
	{0, &Data_Settings.saveGameMissionId, 4},
	{0, &savegameFileVersion, 4},
	{1, &Data_Grid_graphicIds, 52488},
	{1, &Data_Grid_edge, 26244},
	{1, &Data_Grid_buildingIds, 52488},
	{1, &Data_Grid_terrain, 52488},
	{1, &Data_Grid_aqueducts, 26244},
	{1, &Data_Grid_walkerIds, 52488},
	{1, &Data_Grid_bitfields, 26244},
	{1, &Data_Grid_spriteOffsets, 26244},
	{0, &Data_Grid_random, 26244},
	{1, &Data_Grid_desirability, 26244},
	{1, &Data_Grid_elevation, 26244},
	{1, &Data_Grid_buildingDamage, 26244},
	{1, &tmp, 26244}, //{1, &undo_grid_aqueducts, 26244},
	{1, &tmp, 26244}, //{1, &undo_grid_animation, 26244},
	{1, &Data_Walkers, 128000},
	{1, &tmp, 1200}, //{1, &destinationpath_index, 0x4B0},
	{1, &tmp, 300000}, //{1, &destinationpath_data, 0x493E0},
	{1, &Data_Formations, 6400},
	{0, &Data_Formation_Extra.idLastInUse, 4},
	{0, &Data_Formation_Extra.idLastLegion, 4},
	{0, &Data_Formation_Extra.numLegions, 4},
	{1, &Data_CityInfo, 36136},
	{0, &tmp, 2}, //{0, &byte_658DCC, 2},
	{0, &tmp, 64}, //{0, &save_player0name, 0x40},
	{0, &tmp, 4}, //{0, &ciid, 4},
	{1, &Data_Buildings, 256000},
	{0, &Data_Settings_Map.orientation, 4},
	{0, &Data_CityInfo_Extra.gameTimeTick, 4},
	{0, &Data_CityInfo_Extra.gameTimeDay, 4},
	{0, &Data_CityInfo_Extra.gameTimeMonth, 4},
	{0, &Data_CityInfo_Extra.gameTimeYear, 4},
	{0, &Data_CityInfo_Extra.gameTimeTotalDays, 4},
	{0, &Data_Buildings_Extra.highestBuildingIdEver, 4},
	{0, &tmp, 4}, //{0, &dword_98C480, 4},
	{0, &Data_Random.iv1, 4},
	{0, &Data_Random.iv2, 4},
	{0, &Data_Settings_Map.camera.x, 4},
	{0, &Data_Settings_Map.camera.y, 4},
	{0, &Data_CityInfo_Buildings.theater.total, 4},
	{0, &Data_CityInfo_Buildings.theater.working, 4},
	{0, &Data_CityInfo_Buildings.amphitheater.total, 4},
	{0, &Data_CityInfo_Buildings.amphitheater.working, 4},
	{0, &Data_CityInfo_Buildings.colosseum.total, 4},
	{0, &Data_CityInfo_Buildings.colosseum.working, 4},
	{0, &Data_CityInfo_Buildings.hippodrome.total, 4},
	{0, &Data_CityInfo_Buildings.hippodrome.working, 4},
	{0, &Data_CityInfo_Buildings.school.total, 4},
	{0, &Data_CityInfo_Buildings.school.working, 4},
	{0, &Data_CityInfo_Buildings.library.total, 4},
	{0, &Data_CityInfo_Buildings.library.working, 4},
	{0, &Data_CityInfo_Buildings.academy.total, 4},
	{0, &Data_CityInfo_Buildings.academy.working, 4},
	{0, &Data_CityInfo_Buildings.barber.total, 4},
	{0, &Data_CityInfo_Buildings.barber.working, 4},
	{0, &Data_CityInfo_Buildings.bathhouse.total, 4},
	{0, &Data_CityInfo_Buildings.bathhouse.working, 4},
	{0, &Data_CityInfo_Buildings.clinic.total, 4},
	{0, &Data_CityInfo_Buildings.clinic.working, 4},
	{0, &Data_CityInfo_Buildings.hospital.total, 4},
	{0, &Data_CityInfo_Buildings.hospital.working, 4},
	{0, &Data_CityInfo_Buildings.smallTempleCeres.total, 4},
	{0, &Data_CityInfo_Buildings.smallTempleNeptune.total, 4},
	{0, &Data_CityInfo_Buildings.smallTempleMercury.total, 4},
	{0, &Data_CityInfo_Buildings.smallTempleMars.total, 4},
	{0, &Data_CityInfo_Buildings.smallTempleVenus.total, 4},
	{0, &Data_CityInfo_Buildings.largeTempleCeres.total, 4},
	{0, &Data_CityInfo_Buildings.largeTempleNeptune.total, 4},
	{0, &Data_CityInfo_Buildings.largeTempleMercury.total, 4},
	{0, &Data_CityInfo_Buildings.largeTempleMars.total, 4},
	{0, &Data_CityInfo_Buildings.largeTempleVenus.total, 4},
	{0, &Data_CityInfo_Buildings.oracle.total, 4},
	{0, &Data_CityInfo_Extra.populationGraphOrder, 4},
	{0, &tmp, 4}, //{0, &unk_650060, 4},
	{0, &tmp, 4}, //{0, &event_emperorChange_gameYear, 4},
	{0, &tmp, 4}, //{0, &event_emperorChange_month, 4},
	{0, &Data_Empire.scrollX, 4},
	{0, &Data_Empire.scrollY, 4},
	{0, &Data_Empire.selectedObject, 4},
	{1, &Data_Empire_Cities, 2706},
	{0, &Data_CityInfo_Buildings.industry.total, 64},
	{0, &Data_CityInfo_Buildings.industry.working, 64},
	{0, &Data_TradePrices, 128},
	{0, &Data_Walker_NameSequence.citizenMale, 4},
	{0, &Data_Walker_NameSequence.patrician, 4},
	{0, &Data_Walker_NameSequence.citizenFemale, 4},
	{0, &Data_Walker_NameSequence.taxCollector, 4},
	{0, &Data_Walker_NameSequence.engineer, 4},
	{0, &Data_Walker_NameSequence.prefect, 4},
	{0, &Data_Walker_NameSequence.javelinThrower, 4},
	{0, &Data_Walker_NameSequence.cavalry, 4},
	{0, &Data_Walker_NameSequence.legionary, 4},
	{0, &Data_Walker_NameSequence.actor, 4},
	{0, &Data_Walker_NameSequence.gladiator, 4},
	{0, &Data_Walker_NameSequence.lionTamer, 4},
	{0, &Data_Walker_NameSequence.charioteer, 4},
	{0, &Data_Walker_NameSequence.barbarian, 4},
	{0, &Data_Walker_NameSequence.enemyGreek, 4},
	{0, &Data_Walker_NameSequence.enemyEgyptian, 4},
	{0, &Data_Walker_NameSequence.enemyArabian, 4},
	{0, &Data_Walker_NameSequence.trader, 4},
	{0, &Data_Walker_NameSequence.tradeShip, 4},
	{0, &Data_Walker_NameSequence.warShip, 4},
	{0, &Data_Walker_NameSequence.enemyShip, 4},
	{0, &Data_CityInfo_CultureCoverage.theater, 4},
	{0, &Data_CityInfo_CultureCoverage.amphitheater, 4},
	{0, &Data_CityInfo_CultureCoverage.colosseum, 4},
	{0, &Data_CityInfo_CultureCoverage.hospital, 4},
	{0, &Data_CityInfo_CultureCoverage.hippodrome, 4},
	{0, &Data_CityInfo_CultureCoverage.religionCeres, 4},
	{0, &Data_CityInfo_CultureCoverage.religionNeptune, 4},
	{0, &Data_CityInfo_CultureCoverage.religionMercury, 4},
	{0, &Data_CityInfo_CultureCoverage.religionMars, 4},
	{0, &Data_CityInfo_CultureCoverage.religionVenus, 4},
	{0, &Data_CityInfo_CultureCoverage.oracle, 4},
	{0, &Data_CityInfo_CultureCoverage.school, 4},
	{0, &Data_CityInfo_CultureCoverage.library, 4},
	{0, &Data_CityInfo_CultureCoverage.academy, 4},
	{0, &Data_CityInfo_CultureCoverage.hospital, 4},
	{0, &Data_Scenario, 1720},
	{0, &Data_Event.timeLimitMaxGameYear, 4},
	{0, &tmp, 4}, //{0, &event_earthquake_gameYear, 4},
	{0, &tmp, 4}, //{0, &event_earthquake_month, 4},
	{0, &tmp, 4}, //{0, &event_earthquake_state, 4},
	{0, &tmp, 4}, //{0, &dword_89AA8C, 4},
	{0, &tmp, 4}, //{0, &event_earthquake_maxDuration, 4},
	{0, &tmp, 4}, //{0, &event_earthquake_maxDamage, 4},
	{0, &tmp, 4}, //{0, &dword_89AB08, 4},
	{0, &tmp, 32}, //{0, &dword_929660, 0x20},
	{0, &tmp, 4}, //{0, &event_emperorChange_state, 4},
	{1, &Data_Message.messages, 16000},
	{0, &Data_Message.nextMessageSequence, 4},
	{0, &Data_Message.totalMessages, 4},
	{0, &Data_Message.currentMessageId, 4},
	{0, &Data_Message.populationMessagesShown, 0xA},
	{0, &Data_Message.messageCategoryCount, 80},
	{0, &Data_Message.messageDelay, 80},
	{0, &tmp, 4}, //{0, &dword_98BF18, 4},
	{0, &tmp, 4}, //{0, &dword_98C020, 4},
	{0, &tmp, 4}, //{0, &dword_607FC8, 4},
	{0, &tmp, 4}, //{0, &startingFavor, 4},
	{0, &tmp, 4}, //{0, &personalSavings_lastMission, 4},
	{0, &Data_Settings.currentMissionId, 4},
	{1, &Data_InvasionWarnings, 3232},
	{0, &Data_Settings.isCustomScenario, 4},
	{0, &Data_Sound_City, 8960},
	{0, &Data_Buildings_Extra.highestBuildingIdInUse, 4},
	{0, &Data_Walker_Traders, 4800},
	{0, &tmp, 4}, //{0, &dword_990CD8, 4},
	{1, &tmp, 1000}, //{1, &word_98C080, 0x3E8},
	{1, &tmp, 1000}, //{1, &word_949F00, 0x3E8},
	{1, &Data_BuildingList.buildingIds, 4000},
	{0, &Data_Tutorial_tutorial1.fire, 4},
	{0, &Data_Tutorial_tutorial1.crime, 4},
	{0, &Data_Tutorial_tutorial1.collapse, 4},
	{0, &Data_Tutorial_tutorial2.granaryBuilt, 4},
	{0, &Data_Tutorial_tutorial2.population250Reached, 4},
	{0, &Data_Tutorial_tutorial1.senateBuilt, 4},
	{0, &Data_Tutorial_tutorial2.population450Reached, 4},
	{0, &Data_Tutorial_tutorial2.potteryMade, 4},
	{0, &Data_CityInfo_Buildings.militaryAcademy.total, 4},
	{0, &Data_CityInfo_Buildings.militaryAcademy.working, 4},
	{0, &Data_CityInfo_Buildings.barracks.total, 4},
	{0, &Data_CityInfo_Buildings.barracks.working, 4},
	{0, &tmp, 4}, //{0, &dword_819848, 4},
	{0, &tmp, 4}, //{0, &dword_7FA234, 4},
	{0, &tmp, 4}, //{0, &dword_7F87A8, 4},
	{0, &tmp, 4}, //{0, &dword_7F87AC, 4},
	{0, &tmp, 4}, //{0, &dword_863318, 4},
	{0, &Data_Building_Storages, 6400},
	{0, &Data_CityInfo_Buildings.actorColony.total, 4},
	{0, &Data_CityInfo_Buildings.actorColony.working, 4},
	{0, &Data_CityInfo_Buildings.gladiatorSchool.total, 4},
	{0, &Data_CityInfo_Buildings.gladiatorSchool.working, 4},
	{0, &Data_CityInfo_Buildings.lionHouse.total, 4},
	{0, &Data_CityInfo_Buildings.lionHouse.working, 4},
	{0, &Data_CityInfo_Buildings.chariotMaker.total, 4},
	{0, &Data_CityInfo_Buildings.chariotMaker.working, 4},
	{0, &Data_CityInfo_Buildings.market.total, 4},
	{0, &Data_CityInfo_Buildings.market.working, 4},
	{0, &Data_CityInfo_Buildings.reservoir.total, 4},
	{0, &Data_CityInfo_Buildings.reservoir.working, 4},
	{0, &Data_CityInfo_Buildings.fountain.total, 4},
	{0, &Data_CityInfo_Buildings.fountain.working, 4},
	{0, &Data_Tutorial_tutorial2.potteryMadeYear, 4},
	{0, &tmp, 4}, //{0, &event_gladiatorRevolt_gameYear, 4},
	{0, &tmp, 4}, //{0, &event_gladiatorRevolt_month, 4},
	{0, &tmp, 4}, //{0, &event_gladiatorRevold_endMonth, 4},
	{0, &tmp, 4}, //{0, &event_gladiatorRevolt_state, 4},
	{1, &Data_Empire_Trade.maxPerYear, 1280},
	{1, &Data_Empire_Trade.tradedThisYear, 1280},
	{0, &tmp, 4}, //{0, &dword_7FA224, 4},
	{0, &Data_Buildings_Extra.createdSequence, 4},
	{0, &tmp, 4}, //{0, &unk_634474, 4},
	{0, &tmp, 4}, //{0, &dword_614158, 4},
	{0, &tmp, 4}, //{0, &dword_634468, 4},
	{0, &tmp, 4}, //{0, &unk_634470, 4},
	{0, &Data_CityInfo_Buildings.smallTempleCeres.working, 4},
	{0, &Data_CityInfo_Buildings.smallTempleNeptune.working, 4},
	{0, &Data_CityInfo_Buildings.smallTempleMercury.working, 4},
	{0, &Data_CityInfo_Buildings.smallTempleMars.working, 4},
	{0, &Data_CityInfo_Buildings.smallTempleVenus.working, 4},
	{0, &Data_CityInfo_Buildings.largeTempleCeres.working, 4},
	{0, &Data_CityInfo_Buildings.largeTempleNeptune.working, 4},
	{0, &Data_CityInfo_Buildings.largeTempleMercury.working, 4},
	{0, &Data_CityInfo_Buildings.largeTempleMars.working, 4},
	{0, &Data_CityInfo_Buildings.largeTempleVenus.working, 4},
	{0, &tmp, 100}, //{0, &dword_7FA2C0, 0x64},
	{0, &tmp, 100}, //{0, &dword_862DE0, 0x64},
	{0, &tmp, 100}, //{0, &dword_862D60, 0x64},
	{0, &tmp, 100}, //{0, &dword_819860, 0x64},
	{0, &tmp, 100}, //{0, &dword_819760, 0x64},
	{0, &tmp, 100}, //{0, &dword_8197E0, 0x64},
	{0, &tmp, 100}, //{0, &dword_7FA1C0, 0x64},
	{0, &tmp, 100}, //{0, &dword_8198E0, 0x64},
	{0, &tmp, 100}, //{0, &dword_7FA240, 0x64},
	{0, &tmp, 4}, //{0, &dword_607F94, 4},
	{0, &tmp, 4}, //{0, &dword_607F98, 4},
	{0, &tmp, 4}, //{0, &dword_607F9C, 4},
	{0, &tmp, 4}, //{0, &dword_607FA0, 4},
	{0, &tmp, 2}, //{0, &lastInvasionInternalId, 2},
	{0, &Data_Debug.incorrectHousePositions, 4},
	{0, &Data_Debug.unfixableHousePositions, 4},
	{0, &tmp, 65}, //{0, &currentScenarioFilename, 0x41},
	{0, &tmp, 32}, //{0, &mapBookmarks_x, 0x20},
	{0, &Data_Tutorial_tutorial2.disease, 4},
	{0, &tmp, 4}, //{0, &dword_8E7B24, 4},
	{0, &tmp, 4}, //{0, &dword_89AA64, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
	{0, &endMarker, 4},
};

static void setupFromSavedGame();
static int writeCompressedChunk(FILE *fp, const void *buffer, int bytesToWrite);
static int readCompressedChunk(FILE *fp, void *buffer, int bytesToRead);

int GameFile_loadSavedGame(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		return 0;
	}
	Sound_stopMusic();
	for (int i = 0; i < 300 && saveGameParts[i].lengthInBytes; i++) {
		if (saveGameParts[i].compressed) {
			readCompressedChunk(fp, saveGameParts[i].data, saveGameParts[i].lengthInBytes);
		} else {
			fread(saveGameParts[i].data, 1, saveGameParts[i].lengthInBytes, fp);
		}
	}
	fclose(fp);
	
	setupFromSavedGame();
	BuildingStorage_resetBuildingIds();
	strcpy(Data_Settings.playerName, playerNames[1]);
	return 1;
}

int GameFile_loadSavedGameFromMissionPack(int missionId)
{
	int offset;
	if (!FileSystem_readFilePartIntoBuffer(missionPackFile, &offset, 4, 4 * missionId)) {
		return 0;
	}
	if (offset <= 0) {
		return 0;
	}
	FILE *fp = fopen(missionPackFile, "rb");
	if (!fp) {
		return 0;
	}
	fseek(fp, offset, SEEK_SET);
	for (int i = 0; i < 300 && saveGameParts[i].lengthInBytes; i++) {
		if (saveGameParts[i].compressed) {
			readCompressedChunk(fp, saveGameParts[i].data, saveGameParts[i].lengthInBytes);
		} else {
			fread(saveGameParts[i].data, 1, saveGameParts[i].lengthInBytes, fp);
		}
	}
	fclose(fp);

	setupFromSavedGame();
	return 1;
}

static void setupFromSavedGame()
{
	Empire_load(Data_Settings.isCustomScenario, Data_Scenario.empireId);
	Event_calculateDistantBattleRomanTravelTime();
	Event_calculateDistantBattleEnemyTravelTime();

	Data_Settings_Map.width = Data_Scenario.mapSizeX;
	Data_Settings_Map.height = Data_Scenario.mapSizeY;
	Data_Settings_Map.gridStartOffset = Data_Scenario.gridFirstElement;
	Data_Settings_Map.gridBorderSize = Data_Scenario.gridBorderSize;

	if (Data_Settings_Map.orientation >= 0 && Data_Settings_Map.orientation <= 6) {
		// ensure even number
		Data_Settings_Map.orientation = 2 * (Data_Settings_Map.orientation / 2);
	} else {
		Data_Settings_Map.orientation = 0;
	}

	CityView_calculateLookup();
	CityView_checkCameraWithinBounds();

	Routing_clearLandTypeCitizen();
	Routing_determineLandCitizen();
	Routing_determineLandNonCitizen();
	Routing_determineWater();
	Routing_determineWalls();

/*
  j_fun_determineGraphicIdsForOrientatedBuildings();
*/
	WalkerRoute_clean();
/*
  sub_401320();
  j_fun_tick_checkPathingAccessToRome();
  sub_403472();
*/
	SidebarMenu_enableBuildingButtons();
	SidebarMenu_enableBuildingMenuItems();
/*
  j_fun_initPlayerMessageProblemArea();
*/
	Sound_City_init();
	Sound_Music_reset();

	Data_State.undoAvailable = 0;
	Data_State.currentOverlay = 0;
	Data_State.previousOverlay = 0;
/*
  dword_9DA7B0 = 1;
*/
	Data_CityInfo.tutorial1FireMessageShown = 1;
	Data_CityInfo.tutorial2DiseaseMessageShown = 1;

	Loader_Graphics_loadMainGraphics(Data_Scenario.climate);
	Loader_Graphics_loadEnemyGraphics(Data_Scenario.enemyId);
	Empire_determineDistantBattleCity();
/*
  j_fun_determineTerrainGardenFromGraphicIds();
*/
	Data_Message.maxScrollPosition = 0;
	Data_Message.scrollPosition = 0;

	Data_Settings.gamePaused = 0;
}

void GameFile_writeMissionSavedGameIfNeeded(int missionId)
{
	if (missionId < 0) {
		missionId = 0;
	} else if (missionId > 11) {
		missionId = 11;
	}
	if (!Data_CityInfo.missionSavedGameWritten) {
		Data_CityInfo.missionSavedGameWritten = 1;
		if (!FileSystem_fileExists(missionSavedGames[missionId])) {
			GameFile_writeSavedGame(missionSavedGames[missionId]);
		}
	}
}

int GameFile_writeSavedGame(const char *filename)
{
	savegameFileVersion = savegameVersion;
	strcpy(playerNames[1], Data_Settings.playerName);

	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		return 0;
	}
	for (int i = 0; i < 300 && saveGameParts[i].lengthInBytes; i++) {
		if (saveGameParts[i].compressed) {
			writeCompressedChunk(fp, saveGameParts[i].data, saveGameParts[i].lengthInBytes);
		} else {
			fwrite(saveGameParts[i].data, 1, saveGameParts[i].lengthInBytes, fp);
		}
	}
	fclose(fp);
	return 1;
}

int GameFile_deleteSavedGame(const char *filename)
{
	if (remove(filename) == 0) {
		return 1;
	}
	return 0;
}

int GameFile_loadScenario(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		return 1;
	}

	for (int i = 0; i < SCENARIO_PARTS && scenarioParts[i].lengthInBytes > 0; i++) {
		fread(scenarioParts[i].data, 1, scenarioParts[i].lengthInBytes, fp);
	}
    fclose(fp);

	Empire_load(1, Data_Scenario.empireId);
	Event_calculateDistantBattleRomanTravelTime();
	Event_calculateDistantBattleEnemyTravelTime();
    return 0;
}

static int writeCompressedChunk(FILE *fp, const void *buffer, int bytesToWrite)
{
	if (bytesToWrite > COMPRESS_BUFFER_SIZE) {
		return 0;
	}
	int outputSize = COMPRESS_BUFFER_SIZE;
	if (Zip_compress(buffer, bytesToWrite, compressBuffer, &outputSize)) {
		fwrite(&outputSize, 4, 1, fp);
		fwrite(compressBuffer, 1, outputSize, fp);
	} else {
		// unable to compress: write uncompressed
		outputSize = UNCOMPRESSED;
		fwrite(&outputSize, 4, 1, fp);
		fwrite(buffer, 1, bytesToWrite, fp);
	}
    return 1;
}

static int readCompressedChunk(FILE *fp, void *buffer, int bytesToRead)
{
	if (bytesToRead > COMPRESS_BUFFER_SIZE) {
		return 0;
	}
	int inputSize = bytesToRead;
	fread(&inputSize, 4, 1, fp);
	if ((unsigned int) inputSize == UNCOMPRESSED) {
		fread(buffer, 1, bytesToRead, fp);
	} else {
		fread(compressBuffer, 1, inputSize, fp);
		if (!Zip_decompress(compressBuffer, inputSize, buffer, &bytesToRead)) {
			return 0;
		}
	}
	return 1;
}
