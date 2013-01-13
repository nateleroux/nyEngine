#include "..\include.h"
#include "game.h"
#ifdef _WIN32
#include "..\resource.h"
#endif

// The version and platform strings
CVar * g_version, * g_platform;

GameApplication * GameApplication::singleton;

void gameInit()
{
	dbgInit();
	dbgOut("-----nyEngine-----");
	dbgOut("Version: '%s'", dbgVersionNumber());

	g_version = CVar::Create("g_version", dbgVersionNumber(), VAR_READONLY | VAR_NOSYNC | VAR_NOLOAD);

	// Change this to account for any other platforms
#ifdef _WIN32
	g_platform = CVar::Create("g_platform", "windows", VAR_READONLY | VAR_NOSYNC | VAR_NOLOAD);
#endif
	
	CVar::DumpToDebug(true);
}

void gameRun()
{
	GameApplication app;

	gameInit();

#if _DEBUG
	// Compile the maps
	mapCompileAll("sp");
	mapCompilePatch("patch/sp", "sp");
	// mapCompileAll("mp"); // multiplayer???
	// mapCompilePatch("patch/mp", "mp");
	// Repeat for any new major modes that are added
	// No, Free-For-All is not a major mode, it is a gametype in the 'mp' mode
#endif

	app.go();
}

void GameApplication::initializeScene()
{
	// First scene initialization
	// This is where you would set the game to play an intro movie or something
	// As well as initializing any extra game items

	dbgOut("*** Initializing Game ***");

	// Initialize the UI
	gui = new Gui();

	// Initialize scripts and the map system
	luaManager.Init();
	maploader.init(&luaManager);
	mapArch.Init(&maploader);
	mRoot->addResourceLocation("/", mapArch.getType());

	// Shaders
	mRoot->getRenderSystem()->setFixedPipelineEnabled(true);

	// Load our map
	// in the future, this should be in a variable somewhere
	// so we can keep track of it and unload the map when the level is done
	LOADED_MAP map, common;
	maploader.LoadMap("sp_common", &common, true);
	maploader.LoadMap("sp_shootemup", &map);
	//maploader.LoadMap("sp_helloworld", &map);
	maploader.SetupScripts();

	// testing
#if 0
	file f;
	if(f.openWrite("script.sav"))
	{
		luaManager.Tick(1.0 / TICKS_PER_SECOND);
		//CVar::SaveAllVars(f);
		luaManager.save(f);
		f.close();
	}
#endif
	{
		SoundManager->setDistanceModel(AL_LINEAR_DISTANCE);
		OgreOggSound::OgreOggISound * sound = NULL;
		//sound = SoundManager->createSound("NewSound", "sp/helloworld/audio/soundfile.ogg", false, true);
		//sound = SoundManager->createSound("NewSound", "sp/helloworld/audio/soundfile.ogg", true, true);
		//Ogre::SceneNode * node = mSceneMgr->getRootSceneNode()->createChildSceneNode();
		//node->attachObject(sound);
		//audioNode->attachObject(SoundManager->getListener());

		//sound->setReferenceDistance(50);
		//sound->setMaxDistance(1000);
		
		//sound->play();
	}
}

GameApplication::GameApplication()
	: mRoot(NULL),
	mCamera(NULL),
	mSceneMgr(NULL),
	mWindow(NULL),
	mInputManager(NULL),
	mMouse(NULL),
	mKeyboard(NULL),
	deltaTime(0),
	SoundManager(NULL)
{
	singleton = this;
}

GameApplication::~GameApplication()
{
	SoundManager->stopAllSounds();
	mapArch.Shutdown();
	maploader.deinit();
	Ogre::WindowEventUtilities::removeWindowEventListener(mWindow, this);
	windowClosed(mWindow);
	delete gui;
	delete mRoot;

	// Purge all vars last, so that any vars that are used when shutting down dont get borked
	CVar::Purge(true);
}

void GameApplication::chooseSceneManager()
{
	mSceneMgr = mRoot->createSceneManager(Ogre::ST_GENERIC);
}

void GameApplication::createCamera()
{
	// Coordinate notes:
	// Y is vertical, X and Z are horizontal
	// Maya allows you to select either Y-up or Z-up, so this is fine

	// Create the camera and set the clip distance
	mCamera = mSceneMgr->createCamera("PlayerCam");
	mCamera->setNearClipDistance(0.1f);

	// Setup the camera nodes for orientation
	cameraNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
	cameraYawNode = cameraNode->createChildSceneNode();
	cameraPitchNode = cameraYawNode->createChildSceneNode();
	cameraRollNode = cameraPitchNode->createChildSceneNode();
	cameraRollNode->attachObject(mCamera);

	// Create the audio node
	audioNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
	
	// Orient the camera
	cameraNode->setPosition(0, 80, 160);
}

void GameApplication::createFrameListener()
{
	dbgOut("*** Initializing OIS ***");
	OIS::ParamList pl;
	size_t windowHnd = 0;
	std::ostringstream windowHndStr;

	mWindow->getCustomAttribute("WINDOW", &windowHnd);
	windowHndStr << windowHnd;
	pl.insert(std::make_pair(std::string("WINDOW"), windowHndStr.str()));

	mInputManager = OIS::InputManager::createInputSystem(pl);

	mKeyboard = static_cast<OIS::Keyboard*>(mInputManager->createInputObject(OIS::OISKeyboard, true));
	mMouse = static_cast<OIS::Mouse*>(mInputManager->createInputObject(OIS::OISMouse, true));

	mMouse->setEventCallback(this);
	mKeyboard->setEventCallback(this);

	windowResized(mWindow);

	Ogre::WindowEventUtilities::addWindowEventListener(mWindow, this);

	mRoot->addFrameListener(this);
}

void GameApplication::createViewports()
{
	Ogre::Viewport* vp = mWindow->addViewport(mCamera);
	vp->setBackgroundColour(Ogre::ColourValue(0, 0, 1.0f));

	mCamera->setAspectRatio(
		Ogre::Real(vp->getActualWidth()) / Ogre::Real(vp->getActualHeight()));
}

void GameApplication::go()
{
	if(!setup())
		return;

	/*
	updateFromFrame = true;
	mRoot->startRendering();
	return;
	//*/
	updateFromFrame = false;
	mRoot->getRenderSystem()->_initRenderTargets();
	
	Ogre::Timer frameTimer;
	unsigned long long now, lastUpdate;
	double desiredDeltaTime;
	double timePending = 0;
	double fudgeLower, fudgeUpper;
	double dropExcess = 0.2;
	double framerateDelta = 0;
	int framerate = 0;
	int updateCount;
	int desiredFramerate = 60;

	desiredDeltaTime = 1.0 / desiredFramerate;
	//desiredDeltaTime = 0.016f;
	fudgeLower = desiredDeltaTime - (desiredDeltaTime / 20);
	fudgeUpper = desiredDeltaTime + (desiredDeltaTime / 20);

	double addTime;
	bool hasUpdated;
	bool looping = true;

	//lastUpdate = frameTimer.getMilliseconds();
	lastUpdate = frameTimer.getMicrosecondsCPU();

	while(looping)
	{
		Ogre::WindowEventUtilities::messagePump();

		hasUpdated = false;
		updateCount = 0;
		
		// Report framerate
		if(framerateDelta >= 1.0)
		{
			framerateDelta -= 1.0;
			// dbgOut("FPS: %i", framerate);
			framerate = 0;
		}

		//now = frameTimer.getMilliseconds();
		now = frameTimer.getMicrosecondsCPU();
		addTime = (now - lastUpdate) / 1000000.0;
		lastUpdate = now;
		
		if(addTime > dropExcess)
			addTime = 0;

		framerateDelta += addTime;

#if 0
		timePending += addTime;

		if(timePending >= (1.0 / 500.0))
		{
			// deltaTime = timePending;
			deltaTime = 0.016f;
			timePending = 0;
			framerate++;

			if(!tick())
				looping = false;
			if(!mRoot->renderOneFrame())
				looping = false;
		}
#else
		if(addTime >= fudgeLower && addTime <= fudgeUpper)
		{
			deltaTime = desiredDeltaTime;

			if(!tick())
				looping = false;

			updateCount++;
			hasUpdated = true;
			double spare = addTime - desiredDeltaTime;
			if(abs(timePending + spare) < abs(timePending))
				timePending += spare;
		}
		else
		{
			timePending += addTime;

			while(timePending > desiredDeltaTime / 2)
			{
				deltaTime = desiredDeltaTime;
				if(!hasUpdated)
				{
					if(!tick())
						looping = false;
					updateCount++;
				}
				hasUpdated = true;
				timePending -= desiredDeltaTime;
			}
		}

		if(updateCount > 1)
			dbgOut("stutter detected, updated %i times", updateCount);

		if(hasUpdated)
		{
			framerate++;

			if(!mRoot->renderOneFrame())
				looping = false;
		}
#endif
	}
}

bool GameApplication::setup()
{
	// Create the root object
	mRoot = new Ogre::Root("", "", "");

	// Setup the render plugin
	dbgOut("*** Initializing Renderer ***");
#ifdef _DEBUG
#ifdef _WIN32
	mRoot->loadPlugin("RenderSystem_Direct3D9_d.dll");
	//mRoot->loadPlugin("RenderSystem_Direct3D11_d.dll");
#endif // _WIN32
#else // _DEBUG
#ifdef _WIN32
	//mRoot->loadPlugin("RenderSystem_Direct3D11.dll");
	mRoot->loadPlugin("RenderSystem_Direct3D9.dll");
#endif // _WIN32
#endif // _DEBUG

	//Ogre::RenderSystem * rsys = mRoot->getRenderSystemByName("Direct3D11 Rendering Subsystem");
	// Create the rendering subsystem
	Ogre::RenderSystem * rsys = mRoot->getRenderSystemByName("Direct3D9 Rendering Subsystem");
	mRoot->setRenderSystem(rsys);
	mRoot->initialise(false);

	// Create the render window
	Ogre::NameValuePairList windowParms;
	windowParms["border"] = "fixed";
	mWindow = mRoot->createRenderWindow("nyEngine", 640, 480, false, &windowParms);
	mWindow->setVSyncEnabled(true);

#ifdef _WIN32
	// Setup the window icon
	HWND hwnd = 0;
	mWindow->getCustomAttribute("WINDOW", &hwnd);
	if(hwnd == 0)
		dbgError("unable to obtain hwnd of window");

	SetClassLongPtr(hwnd, GCL_HICON, (LONG_PTR)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICONAPP)));
#endif

	// Setup some initial state
	mShutDown = false;
	renderMode = Ogre::TFO_BILINEAR;
	renderModeChanged = true;
	polygonMode = Ogre::PM_SOLID;
	polygonModeChanged = true;

	// Setup the scene/camera/viewports
	chooseSceneManager();
	createCamera();
	createViewports();

	// Setup texture mipmaps
	Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);

	// Setup the audio plugin
	dbgOut("*** Initializing Audio ***");
#ifdef _DEBUG
	mRoot->loadPlugin("OgreOggSound_d.dll");
#else
	mRoot->loadPlugin("OgreOggSound.dll");
#endif

	dbgOut("*** Audio Plugin Loaded ***");

	// Initialize audio
	SoundManager = OgreOggSound::OgreOggSoundManager::getSingletonPtr();
	if(!SoundManager->init("", 100, 64, mSceneMgr))
		dbgError("failed to initialize audio");
	
	dbgOut("*** Audio Initialized ***");

	// Setup the script loader
	new ConfigScriptLoader();

	// Initialize the scene
	initializeScene();

	// Create the frame listener
	createFrameListener();

	// Done!
	return true;
}

bool GameApplication::frameStarted(const Ogre::FrameEvent& evt)
{
	return true;
}

bool GameApplication::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
	if(updateFromFrame)
	{
		deltaTime = evt.timeSinceLastFrame;
		return tick();
	}
	return true;
}

bool GameApplication::tick()
{
	if(mWindow->isClosed())
		return false;

	if(mShutDown)
		return false;
	
	mKeyboard->capture();
	mMouse->capture();

	mouseUpdate(mMouse->getMouseState());

	// We update the scripts here, after input is gathered, but before the physics tick
	// This gives scripts enough time to perform some initial state setup before the first physics tick
	// And before the first frame is rendered
	luaManager.Tick(deltaTime);

	// update camera?
	// update other stuff?
	float movementSpeed = 60;
	Ogre::Vector3 pos = Ogre::Vector3::ZERO;

	if(mKeyboard->isModifierDown(OIS::Keyboard::Modifier::Shift))
		movementSpeed *= 3;

	if(mKeyboard->isKeyDown(OIS::KC_W))
		pos.z -= movementSpeed * deltaTime;
	if(mKeyboard->isKeyDown(OIS::KC_S))
		pos.z += movementSpeed * deltaTime;
	if(mKeyboard->isKeyDown(OIS::KC_D))
		pos.x += movementSpeed * deltaTime;
	if(mKeyboard->isKeyDown(OIS::KC_A))
		pos.x -= movementSpeed * deltaTime;

	cameraNode->translate(cameraYawNode->getOrientation() * cameraPitchNode->getOrientation() * pos, Ogre::SceneNode::TS_LOCAL);
	
	// Fix the audio node to the camera position
	audioNode->setPosition(cameraNode->_getDerivedPosition());
	audioNode->setOrientation(cameraYawNode->getOrientation() *
		cameraPitchNode->getOrientation() *
		cameraRollNode->getOrientation());

	// TODO: Update physics or something

	// Changing the render mode
	if(renderModeChanged)
	{
		uint aniso = 1;
		renderModeChanged = false;

		if(renderMode == Ogre::TFO_ANISOTROPIC)
			aniso = 8;

		Ogre::MaterialManager::getSingleton().setDefaultTextureFiltering((Ogre::TextureFilterOptions)renderMode);
		Ogre::MaterialManager::getSingleton().setDefaultAnisotropy(aniso);
	}

	if(polygonModeChanged)
	{
		polygonModeChanged = false;

		mCamera->setPolygonMode((Ogre::PolygonMode)polygonMode);
	}

	// Now that everything in the scene has been drawn, run the draw scripts
	luaManager.Tick(deltaTime, "@DRAW");

	return true;
}

bool GameApplication::keyPressed(const OIS::KeyEvent& arg)
{
	if(arg.key == OIS::KC_T)
	{
		renderMode++;
		if(renderMode > Ogre::TFO_ANISOTROPIC)
			renderMode = 0;

		renderModeChanged = true;
	}
	else if(arg.key == OIS::KC_R)
	{
		if(polygonMode == Ogre::PM_SOLID)
			polygonMode = Ogre::PM_WIREFRAME;
		else if(polygonMode == Ogre::PM_WIREFRAME)
			polygonMode = Ogre::PM_POINTS;
		else
			polygonMode = Ogre::PM_SOLID;

		polygonModeChanged = true;
	}
	else if(arg.key == OIS::KC_ESCAPE)
		mShutDown = true;

	return true;
}

bool GameApplication::mouseUpdate(const OIS::MouseState& arg)
{
	float cameraUnitsPerSecondPerUnit = 0.6f;

	cameraYawNode->yaw(-Ogre::Radian(Ogre::Real(arg.X.rel) * cameraUnitsPerSecondPerUnit * deltaTime));
	cameraPitchNode->pitch(-Ogre::Radian(Ogre::Real(arg.Y.rel) * cameraUnitsPerSecondPerUnit * deltaTime));
	
	float pitchAngle, pitchAngleSign;
	pitchAngle = (2 * (Ogre::Math::ACos(cameraPitchNode->getOrientation().w))).valueRadians();
	pitchAngleSign = cameraPitchNode->getOrientation().x;

	if(pitchAngle > Ogre::Math::HALF_PI)
	{
		if(pitchAngleSign > 0)
			cameraPitchNode->setOrientation(Ogre::Quaternion(Ogre::Math::Sqrt(0.5f),
                                                                Ogre::Math::Sqrt(0.5f), 0, 0));
     else if (pitchAngleSign < 0)
         this->cameraPitchNode->setOrientation(Ogre::Quaternion(Ogre::Math::Sqrt(0.5f),
                                                                -Ogre::Math::Sqrt(0.5f), 0, 0));
	}

	return true;
}

bool GameApplication::keyReleased(const OIS::KeyEvent& arg)
{
	return true;
}

bool GameApplication::mouseMoved(const OIS::MouseEvent& arg)
{
	return true;
}

bool GameApplication::mousePressed(const OIS::MouseEvent& arg, OIS::MouseButtonID id)
{
	return true;
}

bool GameApplication::mouseReleased(const OIS::MouseEvent& arg, OIS::MouseButtonID id)
{
	return true;
}

void GameApplication::windowResized(Ogre::RenderWindow* rw)
{
	uint width, height, depth;
	int left, top;
	rw->getMetrics(width, height, depth, left, top);

	const OIS::MouseState& ms = mMouse->getMouseState();
	ms.width = width;
	ms.height = height;
}

void GameApplication::windowClosed(Ogre::RenderWindow* rw)
{
	if(rw == mWindow)
	{
		if(mInputManager)
		{
			mInputManager->destroyInputObject(mMouse);
			mInputManager->destroyInputObject(mKeyboard);

			OIS::InputManager::destroyInputSystem(mInputManager);
			mInputManager = NULL;
		}
	}
}
