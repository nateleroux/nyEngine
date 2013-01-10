#ifndef _GAME_H
#define _GAME_H

#include <OgreCamera.h>
#include <OgreEntity.h>
#include <OgreLogManager.h>
#include <OgreRoot.h>
#include <OgreViewport.h>
#include <OgreSceneManager.h>
#include <OgreRenderWindow.h>
#include <OgreConfigFile.h>
#include <OgreWindowEventUtilities.h>
#include <OgreMeshManager.h>
#include <OgreArchiveManager.h>
#include <OgreArchiveFactory.h>
#include <OgreArchive.h>

#include <OISEvents.h>
#include <OISInputManager.h>
#include <OISKeyboard.h>
#include <OISMouse.h>

#include <OgreOggSound.h>

#include "..\util\ConfigScript.h"
#include "..\map\CMapLoader.h"
#include "..\util\LuaManager.h"
#include "..\map\CMapArchive.h"
#include "..\Gorilla\Gui.h"

class GameApplication : public Ogre::FrameListener, public Ogre::WindowEventListener,
	public OIS::KeyListener, public OIS::MouseListener
{
	friend class CLuaManager;

public:
	GameApplication();

	~GameApplication();

	void go();

	static GameApplication * singleton;

    bool setup();
    void chooseSceneManager(void);
    void createCamera(void);
    void createFrameListener(void);
    void initializeScene(void);
    void createViewports(void);
	
	// Ogre::FrameListener
	bool frameStarted(const Ogre::FrameEvent& evt);
    bool frameRenderingQueued(const Ogre::FrameEvent& evt);

    // OIS::KeyListener
    bool keyPressed( const OIS::KeyEvent &arg );
    bool keyReleased( const OIS::KeyEvent &arg );
    // OIS::MouseListener
	bool mouseUpdate( const OIS::MouseState& arg );
    bool mouseMoved( const OIS::MouseEvent &arg );
    bool mousePressed( const OIS::MouseEvent &arg, OIS::MouseButtonID id );
    bool mouseReleased( const OIS::MouseEvent &arg, OIS::MouseButtonID id );

	bool tick();

    // Ogre::WindowEventListener
    // Adjust mouse clipping area
    void windowResized(Ogre::RenderWindow* rw);
    // Unattach OIS before window shutdown (very important under Linux)
    void windowClosed(Ogre::RenderWindow* rw);

	// Audio
	OgreOggSound::OgreOggSoundManager * SoundManager;
	Ogre::SceneNode * audioNode;

	// Various render stuff
    Ogre::Root *mRoot;
    Ogre::Camera* mCamera;
    Ogre::SceneManager* mSceneMgr;
    Ogre::RenderWindow* mWindow;

	// Camera control
	Ogre::SceneNode *cameraNode, *cameraYawNode, *cameraPitchNode, *cameraRollNode;

    // OIS Input devices
    OIS::InputManager* mInputManager;
    OIS::Mouse*    mMouse;
    OIS::Keyboard* mKeyboard;

	// The resource loader
	CMapLoader maploader;
	CMapArchiveFactory mapArch;

	// The script manager
	CLuaManager luaManager;

	// Various variables
	bool mShutDown;
	uint renderMode;
	bool renderModeChanged;
	uint polygonMode;
	bool polygonModeChanged;

	double deltaTime;
	bool updateFromFrame;

	Gui * gui;
};

void gameRun();

#endif