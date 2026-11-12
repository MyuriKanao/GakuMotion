#pragma once
namespace NativeLiveRuntime
{
    namespace r = reflection;
    inline void *objectClass{}, *goClass{}, *typeClass{}, *stringClass{}, *scriptableClass{}, *directorClass{};
    inline void *timelineClass{}, *trackClass{}, *clipClass{}, *bpmClass{}, *equipmentTrackClass{}, *equipmentClass{}, *receiverClass{}, *environmentClass{};
    inline void *sceneLoaderClass{}, *assetLoaderClass{}, *operatorClass{}, *ctsClass{}, *taskClass{}, *awaiterClass{}, *tokenClass{};
    inline void *masterClass{}, *liveMasterClass{}, *liveClass{}, *musicClass{}, *dataClass{}, *sortClass{}, *gradeClass{};
    inline void *listClass{}, *jsonClass{}, *sceneClass{}, *arraySystemClass{};
    inline void *equipmentClipClass{}, *bpmClipClass{}, *groupClass{};
    inline void *clipParentField{}, *trackParentField{}, *clipControllerField{}, *clipTrackField{};
    inline const void *trackParent{}, *addChild{}, *clipUseBpm{};
    inline const void *goCtor{}, *addComponent{}, *getChildren{}, *moveScene{}, *createAsset{}, *cloneObject{}, *destroyObject{};
    inline const void *sceneLoadBundle{}, *sceneLoad{}, *sceneUnload{}, *bundleUnload{}, *sceneCancel{}, *sceneRoots{}, *scenePaths{};
    inline const void *ctsCtor{}, *ctsToken{}, *ctsCancel{}, *ctsDispose{}, *noneToken{}, *taskStatus{}, *getAwaiter{}, *getResult{};
    inline const void *getAsset{}, *setAsset{}, *setManual{}, *setAutoPlay{}, *directorPlay{}, *directorStop{}, *directorEvaluate{}, *directorTime{}, *directorDuration{};
    inline const void *flattenedTracks{}, *trackClips{}, *clearChildren{}, *clearMarkers{}, *addTrack{}, *setParentTrack{}, *clipAsset{}, *setClipAsset{}, *mutedHierarchy{}, *setMuted{};
    inline const void *equipmentInit{}, *equipmentBind{}, *equipmentUpdate{}, *equipmentLate{}, *equipmentReceiver{}, *receiverClear{}, *toJson{}, *fromJson{};
    inline const void *getLocalPosition{}, *setLocalPosition{}, *getLocalRotation{}, *setLocalRotation{}, *getLocalScale{}, *setLocalScale{};
    inline const void *getMaster{}, *getLives{}, *getSongKey{}, *getGrade{}, *getStageKey{}, *getTimelineKey{}, *getMusic{}, *getTitle{};
    inline const void *listCount{}, *listItem{}, *getDatabase{}, *allBundles{}, *dependencies{}, *downloadSize{}, *setArrayValue{};
    inline const void *sceneCount{}, *sceneAt{}, *receiverUpdate{}, *equipmentDispose{};
    inline const void *setWrap{}, *setFixedDuration{}, *setDurationMode{}, *validGraph{};
    inline const void *lookupBundle{}, *getBpmInfo{}, *evaluateBpm{};
    inline void *itemClass{}, *itemNameField{}, *nullableLookupClass{};
    inline int holdMode{}, fixedMode{};
    inline void *loaderField{}, *operatorField{}, *equipmentDirectorField{}, *equipmentReceiverField{}, *loaderSceneField{};
    inline void* (*arrayNew)(void*, uintptr_t){};
    inline int sortOrder{}, manualMode{}, pendingStatus{}, successStatus{};
    inline bool metadataReady{};
    inline std::string error, catalogNotice;

    inline void Resolve(HMODULE module)
    {
        metadataReady = false;
        nullableLookupClass=nullptr;
        try
        {
            r::Resolve(module);
            r::Require(r::stringLength && r::stringChars && r::arrayHeaderSize && r::arrayElementSize, "native string/array ABI exports unavailable");
            arrayNew = reinterpret_cast<decltype(arrayNew)>(GetProcAddress(module, "il2cpp_array_new"));
            objectClass = r::Unity("Object"); goClass = r::Unity("GameObject"); scriptableClass = r::Unity("ScriptableObject");
            typeClass = r::Class("mscorlib.dll", "System", "Type"); stringClass = r::Class("mscorlib.dll", "System", "String");
            arraySystemClass = r::Class("mscorlib.dll", "System", "Array");
            directorClass = r::Class("UnityEngine.DirectorModule.dll", "UnityEngine.Playables", "PlayableDirector");
            auto timeline = [](const char* name) { return r::Class("Unity.Timeline.dll", "UnityEngine.Timeline", name); };
            timelineClass = timeline("TimelineAsset"); trackClass = timeline("TrackAsset"); clipClass = timeline("TimelineClip");
            auto campus = [](const char* ns, const char* name) { return r::Class("campus-submodule.Runtime.dll", ns, name); };
            bpmClass = campus("Campus.Timeline", "CampusBpmTimelineTrack");
            equipmentTrackClass = campus("Campus.Live.Equipment.Timeline", "LiveEquipmentTrackBase");
            equipmentClipClass = campus("Campus.Live.Equipment.Timeline", "LiveEquipmentClipBase");
            bpmClipClass = campus("Campus.Timeline", "CampusBpmTimelineAsset"); groupClass = timeline("GroupTrack");
            equipmentClass = campus("Campus.Live.Equipment", "LiveEquipmentController");
            receiverClass = campus("Campus.Live.Equipment", "LiveEquipmentReceiver");
            environmentClass = campus("Campus.Live.Equipment", "EnvironmentBase");
            sceneLoaderClass = campus("Campus.Common", "AssetBundleSceneLoader");
            assetLoaderClass = r::Class("Octo.dll", "Octo", "AssetLoader");
            operatorClass = r::Class("Octo.dll", "Octo.Asset", "AssetBundleOperator");
            void* operatorInterface = r::Class("Octo.dll", "Octo", "IAssetOperator");
            ctsClass = r::Class("mscorlib.dll", "System.Threading", "CancellationTokenSource");
            tokenClass = StageLoader::tokenClass; taskClass = StageLoader::taskClass;
            awaiterClass = r::Nested(taskClass, "Awaiter");
            jsonClass = r::Class("UnityEngine.JSONSerializeModule.dll", "UnityEngine", "JsonUtility");
            sceneClass = r::Class("UnityEngine.CoreModule.dll", "UnityEngine.SceneManagement", "Scene");
            auto master = [](const char* name) { return r::Class("Assembly-CSharp.dll", "Campus.Common.Master", name); };
            auto row = [](const char* name) { return r::Class("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master", name); };
            masterClass = master("MasterManager"); liveMasterClass = master("ProduceLiveMaster"); sortClass = master("ProduceLiveSortType");
            liveClass = row("ProduceLive"); musicClass = row("Music");
            dataClass = r::Class("Octo.dll", "Octo.Data", "DataManager");
            gradeClass = r::Class("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Enums", "ProduceLiveType");
            r::Require(r::arrayClass && r::objectNew && arrayNew && r::handleTarget && r::Layout(sceneClass, 4, 4), "native lifetime/scene metadata unavailable");
            using T = r::Type;
            auto find = [&](const void*& slot, void* owner, const char* name, bool stat, T result, std::initializer_list<T> args)
            { slot = r::Find(owner, name, stat, result, args); r::Require(slot != nullptr, name); };
            auto arr = [](void* klass) { return klass && r::arrayClass ? r::arrayClass(klass, 1) : nullptr; };
            find(goCtor, goClass, ".ctor", false, {0x01}, {{0x0e}});
            find(addComponent, goClass, "AddComponent", false, {0x12, StageLoader::componentClass}, {{0x12, typeClass}});
            find(getChildren, goClass, "GetComponentsInChildren", false, {0x1d, arr(StageLoader::componentClass)}, {{0x12, typeClass}, {0x02}});
            find(moveScene, r::Class("UnityEngine.CoreModule.dll", "UnityEngine.SceneManagement", "SceneManager"), "MoveGameObjectToScene", true,
                {0x01}, {{0x12, goClass}, {0x11, sceneClass}});
            void* sceneManager = r::Class("UnityEngine.CoreModule.dll", "UnityEngine.SceneManagement", "SceneManager");
            find(sceneCount, sceneManager, "get_sceneCount", true, {0x08}, {});
            find(sceneAt, sceneManager, "GetSceneAt", true, {0x11, sceneClass}, {{0x08}});
            find(createAsset, scriptableClass, "CreateInstance", true, {0x12, scriptableClass}, {{0x12, typeClass}});
            find(cloneObject, objectClass, "Instantiate", true, {0x12, objectClass}, {{0x12, objectClass}});
            find(destroyObject, objectClass, "Destroy", true, {0x01}, {{0x12, objectClass}});
            find(sceneLoadBundle, sceneLoaderClass, "LoadAssetBundleAsync", false, {0x11, taskClass}, {{0x0e}, {0x11, tokenClass}});
            find(sceneLoad, sceneLoaderClass, "LoadSceneFromLoadedAssetBundleAsync", false, {0x11, taskClass}, {{0x0e}, {0x11, tokenClass}});
            find(sceneUnload, sceneLoaderClass, "UnloadSceneAsync", false, {0x11, taskClass}, {{0x11, tokenClass}});
            find(bundleUnload, sceneLoaderClass, "UnloadAssetBundleAsync", false, {0x11, taskClass}, {{0x11, tokenClass}});
            find(sceneCancel, sceneLoaderClass, "CancelLoad", false, {0x01}, {});
            find(sceneRoots, sceneLoaderClass, "get_RootObjects", false, {0x1d, arr(goClass)}, {});
            find(scenePaths, operatorClass, "GetAllScenePaths", false, {0x1d, arr(stringClass)}, {});
            loaderField = r::Field(sceneLoaderClass, "_loader", {0x12, assetLoaderClass});
            operatorField = r::Field(assetLoaderClass, "_operator", {0x12, operatorInterface});
            loaderSceneField = r::Field(sceneLoaderClass, "_currentScene", {0x11, sceneClass});
            r::Require(loaderField && operatorField && loaderSceneField, "scene loader ownership fields unavailable");
            find(ctsCtor, ctsClass, ".ctor", false, {0x01}, {});
            find(ctsToken, ctsClass, "get_Token", false, {0x11, tokenClass}, {});
            find(ctsCancel, ctsClass, "Cancel", false, {0x01}, {});
            find(ctsDispose, ctsClass, "Dispose", false, {0x01}, {});
            find(noneToken, tokenClass, "get_None", true, {0x11, tokenClass}, {});
            void* statusClass = r::Class("UniTask.dll", "Cysharp.Threading.Tasks", "UniTaskStatus");
            pendingStatus = r::Enum(statusClass, "Pending"); successStatus = r::Enum(statusClass, "Succeeded");
            find(taskStatus, taskClass, "get_Status", false, {0x11, statusClass}, {});
            find(getAwaiter, taskClass, "GetAwaiter", false, {0x11, awaiterClass}, {});
            find(getResult, awaiterClass, "GetResult", false, {0x01}, {});
            void* playableAsset = r::Class("UnityEngine.CoreModule.dll", "UnityEngine.Playables", "PlayableAsset");
            find(getAsset, directorClass, "get_playableAsset", false, {0x12, playableAsset}, {});
            find(setAsset, directorClass, "set_playableAsset", false, {0x01}, {{0x12, playableAsset}});
            void* updateMode = r::Class("UnityEngine.CoreModule.dll", "UnityEngine.Playables", "DirectorUpdateMode");
            manualMode = r::Enum(updateMode, "Manual");
            find(setManual, directorClass, "set_timeUpdateMode", false, {0x01}, {{0x11, updateMode}});
            find(setAutoPlay, directorClass, "set_playOnAwake", false, {0x01}, {{0x02}});
            find(directorPlay, directorClass, "Play", false, {0x01}, {});
            find(directorStop, directorClass, "Stop", false, {0x01}, {});
            find(directorEvaluate, directorClass, "Evaluate", false, {0x01}, {});
            find(directorTime, directorClass, "set_time", false, {0x01}, {{0x0d}});
            find(directorDuration, directorClass, "get_duration", false, {0x0d}, {});
            void* wrapClass = r::Class("UnityEngine.CoreModule.dll", "UnityEngine.Playables", "DirectorWrapMode");
            holdMode = r::Enum(wrapClass, "Hold");
            find(setWrap, directorClass, "set_extrapolationMode", false, {0x01}, {{0x11, wrapClass}});
            void* durationMode = r::Nested(timelineClass, "DurationMode"); fixedMode = r::Enum(durationMode, "FixedLength");
            find(setFixedDuration, timelineClass, "set_fixedDuration", false, {0x01}, {{0x0d}});
            find(setDurationMode, timelineClass, "set_durationMode", false, {0x01}, {{0x11, durationMode}});
            find(flattenedTracks, timelineClass, "get_flattenedTracks", false, {0x1d, arr(trackClass)}, {});
            find(trackClips, trackClass, "get_clips", false, {0x1d, arr(clipClass)}, {});
            find(clearChildren, trackClass, "ClearSubTracksInternal", false, {0x01}, {});
            find(clearMarkers, trackClass, "ClearMarkers", false, {0x01}, {});
            find(addTrack, timelineClass, "AddTrackInternal", false, {0x01}, {{0x12, trackClass}});
            find(setParentTrack, clipClass, "set_parentTrack", false, {0x01}, {{0x12, trackClass}});
            find(trackParent, trackClass, "get_parent", false, {0x12, playableAsset}, {});
            find(addChild, trackClass, "AddChild", false, {0x01}, {{0x12, trackClass}});
            find(clipUseBpm, equipmentClipClass, "get_UseBpm", false, {0x02}, {});
            clipParentField = r::Field(clipClass, "m_ParentTrack", {0x12, trackClass});
            trackParentField = r::Field(trackClass, "m_Parent", {0x12, playableAsset});
            clipControllerField = r::Field(equipmentClipClass, "controller", {0x12, equipmentClass});
            clipTrackField = r::Field(equipmentClipClass, "track", {0x12, equipmentTrackClass});
            r::Require(clipParentField && trackParentField && clipControllerField && clipTrackField, "cloned track parent fields unavailable");
            find(clipAsset, clipClass, "get_asset", false, {0x12, objectClass}, {});
            find(setClipAsset, clipClass, "set_asset", false, {0x01}, {{0x12, objectClass}});
            find(mutedHierarchy, trackClass, "get_mutedInHierarchy", false, {0x02}, {});
            find(setMuted, trackClass, "set_muted", false, {0x01}, {{0x02}});
            equipmentDirectorField = r::Field(equipmentClass, "_director", {0x12, directorClass});
            equipmentReceiverField = r::Field(equipmentClass, "_receiver", {0x12, receiverClass});
            r::Require(equipmentDirectorField && equipmentReceiverField, "equipment owner fields unavailable");
            find(equipmentInit, equipmentClass, "Initialize", false, {0x01}, {});
            find(equipmentBind, equipmentClass, "BindReceiver", false, {0x01}, {{0x12, receiverClass}});
            find(equipmentReceiver, equipmentClass, "get_Receiver", false, {0x12, receiverClass}, {});
            find(equipmentUpdate, equipmentClass, "Update", false, {0x01}, {});
            find(equipmentLate, equipmentClass, "LateUpdate", false, {0x01}, {});
            find(receiverClear, receiverClass, "ClearProperty", false, {0x01}, {});
            find(receiverUpdate, receiverClass, "Update", false, {0x01}, {});
            find(equipmentDispose, equipmentClass, "Dispose", false, {0x01}, {});
            find(validGraph, equipmentClass, "get_IsValidPlayableGraph", false, {0x02}, {});
            void* bpmInfo=campus("Campus.Timeline", "CampusBpmInfo");
            find(getBpmInfo,equipmentTrackClass,"get_BpmInfo",false,{0x12,bpmInfo},{});
            find(evaluateBpm,bpmInfo,"EvaluateBpmAtTime",false,{0x0c},{{0x0c}});
            find(toJson, jsonClass, "ToJson", true, {0x0e}, {{0x1c}, {0x02}});
            // This player's public FromJsonOverwrite wrapper is stripped. Its
            // exact internal endpoint retains the same existing-object path.
            find(fromJson, jsonClass, "FromJsonInternal", true, {0x1c}, {{0x0e}, {0x1c}, {0x12, typeClass}});
            void* vector = r::Unity("Vector3"); void* quat = r::Unity("Quaternion");
            r::Require(r::Layout(vector, 12, 4) && r::Layout(quat, 16, 4), "transform value layout unavailable");
            find(getLocalPosition, StageLoader::transformClass, "get_localPosition", false, {0x11, vector}, {});
            find(setLocalPosition, StageLoader::transformClass, "set_localPosition", false, {0x01}, {{0x11, vector}});
            find(getLocalRotation, StageLoader::transformClass, "get_localRotation", false, {0x11, quat}, {});
            find(setLocalRotation, StageLoader::transformClass, "set_localRotation", false, {0x01}, {{0x11, quat}});
            find(getLocalScale, StageLoader::transformClass, "get_localScale", false, {0x11, vector}, {});
            find(setLocalScale, StageLoader::transformClass, "set_localScale", false, {0x01}, {{0x11, vector}});
            find(getMaster, masterClass, "get_ProduceLiveMaster", true, {0x12, liveMasterClass}, {});
            sortOrder = r::Enum(sortClass, "MusicId_Asc");
            getLives = r::Find(liveMasterClass, "GetAllWithSortByKey", false, {0x15}, {{0x11, sortClass}});
            r::Require(getLives != nullptr, "native live table method unavailable");
            listClass = StageLoader::metadata.classFromType(StageLoader::metadata.result(getLives));
            listCount = r::Find(listClass, "get_Count", false, {0x08}, {}, true);
            listItem = r::Find(listClass, "get_Item", false, {0x12, liveClass}, {{0x08}}, true);
            r::Require(listCount && listItem, "typed native live list unavailable");
            find(getSongKey, liveClass, "get_MusicId", false, {0x0e}, {});
            find(getGrade, liveClass, "get_Type", false, {0x11, gradeClass}, {});
            r::Require(r::Layout(gradeClass, 4, 4), "native live grade layout unavailable");
            find(getStageKey, liveClass, "get_EnvironmentAssetId", false, {0x0e}, {});
            find(getTimelineKey, liveClass, "get_TimelineAssetId", false, {0x0e}, {});
            find(getMusic, liveClass, "GetMusic", false, {0x12, musicClass}, {});
            find(getTitle, musicClass, "GetDisplayTitle", false, {0x0e}, {});
            void* octo = r::Class("Octo.dll", "Octo", "OctoManager");
            find(getDatabase, octo, "get_Database", true, {0x12, r::Class("Octo.dll", "Octo", "IDatabase")}, {});
            find(allBundles, dataClass, "GetAllAssetBundleNames", false, {0x1d, arr(stringClass)}, {});
            itemClass=r::Class("Octo.dll","Octo.Data","Item");
            itemNameField=r::Field(itemClass,"name",{0x0e});
            lookupBundle=r::Find(dataClass,"GetAssetBundleItemByName",false,{0x12,itemClass},{{0x0e},{0x02}});
            if (!lookupBundle)
            {
                lookupBundle=r::Find(dataClass,"GetAssetBundleItemByName",false,{0x12,itemClass},{{0x0e},{0x15}});
                r::Require(lookupBundle!=nullptr,"native bundle lookup unavailable");
                nullableLookupClass=StageLoader::metadata.classFromType(StageLoader::metadata.parameter(lookupBundle,1));
                r::Require(nullableLookupClass && r::Layout(nullableLookupClass,2,1) &&
                    r::Field(nullableLookupClass,"hasValue",{0x02}) && r::Field(nullableLookupClass,"value",{0x02}),"bundle lookup nullable bool layout mismatch");
            }
            r::Require(lookupBundle && itemNameField,"native bundle lookup name unavailable");
            find(dependencies, octo, "GetAllDependenciesOfAssetBundle", true, {0x1d, arr(stringClass)}, {{0x0e}});
            downloadSize = r::Find(octo, "GetTotalAssetBundlesSizeToDownload", true, {0x0a}, {{0x15}});
            r::Require(downloadSize != nullptr, "local asset-cache check unavailable");
            void* enumerable = StageLoader::metadata.classFromType(StageLoader::metadata.parameter(downloadSize, 0));
            r::Require(enumerable && r::className && r::classNamespace &&
                !std::strcmp(r::className(enumerable), "IEnumerable`1") && !std::strcmp(r::classNamespace(enumerable), "System.Collections.Generic") &&
                StageLoader::assignable && StageLoader::assignable(enumerable, arr(stringClass)), "asset-cache parameter is not IEnumerable<string>");
            find(setArrayValue, arraySystemClass, "SetValue", false, {0x01}, {{0x1c}, {0x08}});
            metadataReady = true; error.clear();
        }
        catch (const std::exception& problem) { error = problem.what(); }
        GakuMotion::host::Info("[GakuMotion][NativeLive] exact metadata ready=%d reason=%s (visual acceptance pending)", metadataReady, error.c_str());
    }
}
