#pragma once
// Key names and "$schema" ids of the 2Ship Unbound format, spelled once for the factories and the converter.
// The contract is unbound-docs/SPEC.md; a key not listed here is not part of the format.

namespace S2H::Unbound::Schema {

inline constexpr const char* kSceneType = "2ship-unbound/scene";
inline constexpr const char* kRoomType = "2ship-unbound/room";
inline constexpr const char* kSceneV1 = "2ship-unbound/scene/1";
inline constexpr const char* kRoomV1 = "2ship-unbound/room/1";
inline constexpr int kSceneVersion = 1;

inline constexpr const char* kSchema = "$schema";
inline constexpr const char* kOrder = "$order";
inline constexpr const char* kReplace = "$replace";

inline constexpr const char* kSetups = "setups";
inline constexpr const char* kCommandOrder = "order";

// Setup keys, one per scene command (SPEC §4.2)
inline constexpr const char* kSpawns = "spawns";
inline constexpr const char* kActors = "actors";
inline constexpr const char* kActorCsCameras = "actorCsCameras";
inline constexpr const char* kCollision = "collision";
inline constexpr const char* kRooms = "rooms";
inline constexpr const char* kWind = "wind";
inline constexpr const char* kEntrances = "entrances";
inline constexpr const char* kSpecialObjects = "specialObjects";
inline constexpr const char* kBehavior = "behavior";
inline constexpr const char* kMesh = "mesh";
inline constexpr const char* kObjects = "objects";
inline constexpr const char* kLights = "lights";
inline constexpr const char* kPaths = "paths";
inline constexpr const char* kTransitionActors = "transitionActors";
inline constexpr const char* kLighting = "lighting";
inline constexpr const char* kTime = "time";
inline constexpr const char* kSkybox = "skybox";
inline constexpr const char* kSkyboxModifier = "skyboxModifier";
inline constexpr const char* kExits = "exits";
inline constexpr const char* kSound = "sound";
inline constexpr const char* kEcho = "echo";
inline constexpr const char* kCutscenes = "cutscenes";
inline constexpr const char* kRegionVisited = "regionVisited";
inline constexpr const char* kAnimatedMaterials = "animatedMaterials";
inline constexpr const char* kActorCutscenes = "actorCutscenes";
inline constexpr const char* kMinimap = "minimap";
inline constexpr const char* kMinimapChests = "minimapChests";

// Entity fields
inline constexpr const char* kId = "id";
inline constexpr const char* kPos = "pos";
inline constexpr const char* kRot = "rot";
inline constexpr const char* kRotY = "rotY";
inline constexpr const char* kParams = "params";
inline constexpr const char* kType = "type";
inline constexpr const char* kColor = "color";
inline constexpr const char* kGlow = "glow";
inline constexpr const char* kRadius = "radius";
inline constexpr const char* kSpawn = "spawn";
inline constexpr const char* kRoom = "room";
inline constexpr const char* kFront = "front";
inline constexpr const char* kBack = "back";
inline constexpr const char* kEffects = "effects";
inline constexpr const char* kSetting = "setting";
inline constexpr const char* kPoints = "points";
inline constexpr const char* kWest = "west";
inline constexpr const char* kVertical = "vertical";
inline constexpr const char* kSouth = "south";
inline constexpr const char* kSpeed = "speed";
inline constexpr const char* kElfMessage = "elfMessage";
inline constexpr const char* kGlobalObject = "globalObject";
inline constexpr const char* kGameplayFlags = "gameplayFlags";
inline constexpr const char* kCurrRoomUnk2 = "currRoomUnk2";
inline constexpr const char* kCurrRoomUnk5 = "currRoomUnk5";
inline constexpr const char* kMsgCtxUnk = "msgCtxUnk";
inline constexpr const char* kEnablePointLights = "enablePointLights";
inline constexpr const char* kKankyoContextUnkE2 = "kankyoContextUnkE2";
inline constexpr const char* kHour = "hour";
inline constexpr const char* kMinute = "minute";
inline constexpr const char* kIncrement = "increment";
inline constexpr const char* kUnk = "unk";
inline constexpr const char* kWeather = "weather";
inline constexpr const char* kIndoors = "indoors";
inline constexpr const char* kSkyboxDisabled = "skyboxDisabled";
inline constexpr const char* kSunMoonDisabled = "sunMoonDisabled";
inline constexpr const char* kSeq = "seq";
inline constexpr const char* kNatureAmbience = "natureAmbience";
inline constexpr const char* kReverb = "reverb";
inline constexpr const char* kScript = "script";
inline constexpr const char* kExit = "exit";
inline constexpr const char* kEntrance = "entrance";
inline constexpr const char* kFlag = "flag";

// Lighting entry
inline constexpr const char* kAmbient = "ambient";
inline constexpr const char* kLight1Dir = "light1Dir";
inline constexpr const char* kLight1Color = "light1Color";
inline constexpr const char* kLight2Dir = "light2Dir";
inline constexpr const char* kLight2Color = "light2Color";
inline constexpr const char* kFogColor = "fogColor";
inline constexpr const char* kFogNear = "fogNear";
inline constexpr const char* kFogFar = "fogFar";
inline constexpr const char* kFogStart = "fogStart";
inline constexpr const char* kFogEnd = "fogEnd";
inline constexpr const char* kDrawDistance = "drawDistance";
inline constexpr const char* kNearPlane = "nearPlane";

// Actor cutscene entry
inline constexpr const char* kPriority = "priority";
inline constexpr const char* kLength = "length";
inline constexpr const char* kCsCamId = "csCamId";
inline constexpr const char* kScriptIndex = "scriptIndex";
inline constexpr const char* kAdditionalCsId = "additionalCsId";
inline constexpr const char* kEndSfx = "endSfx";
inline constexpr const char* kCustomValue = "customValue";
inline constexpr const char* kHudVisibility = "hudVisibility";
inline constexpr const char* kEndCam = "endCam";
inline constexpr const char* kLetterboxSize = "letterboxSize";

// Minimap
inline constexpr const char* kScale = "scale";
inline constexpr const char* kMapId = "mapId";
inline constexpr const char* kUnk0 = "unk0";
inline constexpr const char* kUnk2 = "unk2";
inline constexpr const char* kUnk4 = "unk4";
inline constexpr const char* kUnk6 = "unk6";
inline constexpr const char* kUnk8 = "unk8";

// Room shape (SPEC §4.3)
inline constexpr const char* kEntries = "entries";
inline constexpr const char* kOpa = "opa";
inline constexpr const char* kXlu = "xlu";
inline constexpr const char* kFormat = "format";
inline constexpr const char* kImages = "images";
inline constexpr const char* kUnk00 = "unk00";
inline constexpr const char* kSource = "source";
inline constexpr const char* kUnk0C = "unk0C";
inline constexpr const char* kTlut = "tlut";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kHeight = "height";
inline constexpr const char* kFmt = "fmt";
inline constexpr const char* kSiz = "siz";
inline constexpr const char* kMode0 = "mode0";
inline constexpr const char* kTlutCount = "tlutCount";

// Collision document (SPEC §4.5)
inline constexpr const char* kCollisionType = "2ship-unbound/collision";
inline constexpr int kCollisionVersion = 1;
inline constexpr const char* kBounds = "bounds";
inline constexpr const char* kMin = "min";
inline constexpr const char* kMax = "max";
inline constexpr const char* kBulk = "bulk";
inline constexpr const char* kFile = "file";
inline constexpr const char* kVertices = "vertices";
inline constexpr const char* kPolys = "polys";
inline constexpr const char* kSurfaceTypes = "surfaceTypes";
inline constexpr const char* kCameras = "cameras";
inline constexpr const char* kCameraPositions = "cameraPositions";
inline constexpr const char* kWaterBoxes = "waterBoxes";
inline constexpr const char* kCount = "count";
inline constexpr const char* kPositionIndex = "positionIndex";
inline constexpr const char* kCamera = "camera";
inline constexpr const char* kFloorType = "floorType";
inline constexpr const char* kWallFlags = "wallFlags";
inline constexpr const char* kWallType = "wallType";
inline constexpr const char* kFloorProperty = "floorProperty";
inline constexpr const char* kIsSoft = "isSoft";
inline constexpr const char* kIsHorseBlocked = "isHorseBlocked";
inline constexpr const char* kMaterial = "material";
inline constexpr const char* kFloorEffect = "floorEffect";
inline constexpr const char* kLightSetting = "lightSetting";
inline constexpr const char* kCanHookshot = "canHookshot";
inline constexpr const char* kConveyorSpeed = "conveyorSpeed";
inline constexpr const char* kConveyorDirection = "conveyorDirection";
inline constexpr const char* kIsWallDamage = "isWallDamage";
inline constexpr const char* kXMin = "xMin";
inline constexpr const char* kYSurface = "ySurface";
inline constexpr const char* kZMin = "zMin";
inline constexpr const char* kXLength = "xLength";
inline constexpr const char* kZLength = "zLength";
inline constexpr const char* kNotSwimmable = "notSwimmable";

// Scene registry (SPEC §7)
inline constexpr const char* kRegistryPath = "unbound/scenes.json";
inline constexpr const char* kName = "name";
inline constexpr const char* kScene = "scene";
inline constexpr const char* kDrawConfig = "drawConfig";
inline constexpr const char* kTitleTextId = "titleTextId";
inline constexpr const char* kTitleCard = "titleCard";
inline constexpr const char* kStartTransition = "startTransition";
inline constexpr const char* kEndTransition = "endTransition";
inline constexpr const char* kContinueBgm = "continueBgm";
inline constexpr const char* kPersistentFlags = "persistentFlags";
inline constexpr const char* kChest = "chest";
inline constexpr const char* kSwitch0 = "switch0";
inline constexpr const char* kSwitch1 = "switch1";
inline constexpr const char* kCollectible = "collectible";

// Manifest (SPEC §6)
inline constexpr const char* kManifestPath = "unbound.json";
inline constexpr const char* kFormatName = "format";
inline constexpr const char* kFormatValue = "2ship-unbound";
inline constexpr const char* kFormatVersion = "formatVersion";
inline constexpr const char* kGame = "game";
inline constexpr const char* kFeatures = "features";
inline constexpr const char* kFeatureScenes = "scenes";
inline constexpr const char* kSourceInfo = "source";
inline constexpr const char* kConverter = "converter";
inline constexpr const char* kRomHashes = "romHashes";
inline constexpr int kCurrentFormatVersion = 1;

} // namespace S2H::Unbound::Schema
