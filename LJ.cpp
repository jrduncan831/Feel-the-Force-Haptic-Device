//==============================================================================
/*
	Software License Agreement (BSD License)
	Copyright (c) 2003-2016, CHAI3D.
	(www.chai3d.org)

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	* Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

	* Redistributions in binary form must reproduce the above
	copyright notice, this list of conditions and the following
	disclaimer in the documentation and/or other materials provided
	with the distribution.

	* Neither the name of CHAI3D nor the names of its contributors may
	be used to endorse or promote products derived from this software
	without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.

	\author    <http://www.chai3d.org>
	\author    Francois Conti
	\version   3.2.0 $Rev: 1922 $
*/
//==============================================================================

//------------------------------------------------------------------------------
#include "chai3d.h"
#include "atom.h"
//------------------------------------------------------------------------------
#include <GLFW/glfw3.h>
#include <math.h>
#include <ctime>
#include <chrono>
#include <fstream>
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// GENERAL SETTINGS
//------------------------------------------------------------------------------

// Stereo Mode
/*
	C_STEREO_DISABLED:            Stereo is disabled
	C_STEREO_ACTIVE:              Active stereo for OpenGL NVIDIA QUADRO cards
	C_STEREO_PASSIVE_LEFT_RIGHT:  Passive stereo where L/R images are rendered next to each other
	C_STEREO_PASSIVE_TOP_BOTTOM:  Passive stereo where L/R images are rendered above each other
*/
cStereoMode stereoMode = C_STEREO_DISABLED;

// Fullscreen mode
bool fullscreen = false;

//------------------------------------------------------------------------------
// STATES
//------------------------------------------------------------------------------
enum MouseState
{
    MOUSE_IDLE,
    MOUSE_SELECTION
};

//------------------------------------------------------------------------------
// DECLARED CONSTANTS
//------------------------------------------------------------------------------

// Number of spheres in the scene
//const int NUM_SPHERES = 5;

// Radius of each sphere
const double SPHERE_RADIUS = 0.008;

//number of cameras
const int NUM_CAM = 2;

//------------------------------------------------------------------------------
// DECLARED VARIABLES
//------------------------------------------------------------------------------

// a world that contains all objects of the virtual environment
cWorld *world;

// a camera to render the world in the window display
cCamera *camera;

// a light source to illuminate the objects in the world
cSpotLight *light;

//cToolCursor* tool;

// a haptic device handler
cHapticDeviceHandler *handler;

// a pointer to the current haptic device
cGenericHapticDevicePtr hapticDevice;

// highest stiffness the current haptic device can render
double hapticDeviceMaxStiffness;

// sphere objects
vector<Atom*> spheres;

// a colored background
cBackground *background;

// a font for rendering text
cFontPtr font;

// a label to display the rate [Hz] at which the simulation is running
cLabel *labelRates;

//a label to show the potential energy
cLabel *LJ_num;

//a label to display the total energy of the system
cLabel *total_energy;

// a flag that indicates if the haptic simulation is currently running
bool simulationRunning = false;

// a flag that indicates if the haptic simulation has terminated
bool simulationFinished = true;

// mouse state
MouseState mouseState = MOUSE_IDLE;

// a frequency counter to measure the simulation graphic rate
cFrequencyCounter freqCounterGraphics;

// a frequency counter to measure the simulation haptic rate
cFrequencyCounter freqCounterHaptics;

// haptic thread
cThread *hapticsThread;

// a handle to window display context
GLFWwindow *window = NULL;

// current width of window
int width = 0;

// current height of window
int height = 0;

// swap interval for the display context (vertical synchronization)
int swapInterval = 1;

// root resource path
string resourceRoot;

// a scope to monitor the potential energy
cScope *scope;

// global minimum for the given cluster size
double global_minimum;

// a pointer to the selected object
Atom* selectedAtom = NULL;

// offset between the position of the mmouse click on the object and the object reference frame location.
cVector3d selectedAtomOffset;

// position of mouse click.
cVector3d selectedPoint;

//------------------------------------------------------------------------------
// DECLARED MACROS
//------------------------------------------------------------------------------
// convert to resource path
#define RESOURCE_PATH(p) (char *)((resourceRoot + string(p)).c_str())

//------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//------------------------------------------------------------------------------

// callback when the window display is resized
void windowSizeCallback(GLFWwindow *a_window, int a_width, int a_height);

// callback when an error GLFW occurs
void errorCallback(int error, const char *a_description);

// callback when a key is pressed
void keyCallback(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods);

// callback to handle mouse click
void mouseButtonCallback(GLFWwindow* a_window, int a_button, int a_action, int a_mods);

//callback to handle mouse motion
void mouseMotionCallback(GLFWwindow* a_window, double a_posX, double a_posY);

// this function renders the scene
void updateGraphics(void);

// this function contains the main haptics simulation loop
void updateHaptics(void);

// this function closes the application
void close(void);

// Reads in global minimum from global_minima.txt
double getGlobalMinima(int cluster_size);


//------------------------------------------------------------------------------
// DECLARED MACROS
//------------------------------------------------------------------------------
// Convert to resource path
#define RESOURCE_PATH(p) (char *)((resourceRoot + string(p)).c_str())

//==============================================================================
/*
	LJ.cpp

	This program simulates LJ clusters of varying sizes using modified sphere
	primitives (atom.cpp). All dynamics and collisions are computed in the
	haptics thread.
*/
//==============================================================================
int curr_camera = 1;
double theta = 0;
double x = .5;
double y = .5;

int main(int argc, char *argv[])
{
	//--------------------------------------------------------------------------
	// INITIALIZATION
	//--------------------------------------------------------------------------
	cout << endl;
	cout << "-----------------------------------" << endl;
	cout << "CHAI3D" << endl;
	cout << "LJ.cpp" << endl;

	cout << "-----------------------------------" << endl
		 << endl
		 << endl;
	cout << endl
		 << endl;

	//--------------------------------------------------------------------------
	// OPEN GL - WINDOW DISPLAY
	//--------------------------------------------------------------------------

	// initialize GLFW library
	if (!glfwInit())
	{
		cout << "failed initialization" << endl;
		cSleepMs(1000);
		return 1;
	}

	// set error callback
	glfwSetErrorCallback(errorCallback);

	// compute desired size of window
	const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	//	const w *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	int w = 0.8 * mode->height;
	int h = 0.5 * mode->height;
	int x = 0.5 * (mode->width - w);
	int y = 0.5 * (mode->height - h);

	// set OpenGL version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

	// set active stereo mode
	if (stereoMode == C_STEREO_ACTIVE)
	{
		glfwWindowHint(GLFW_STEREO, GL_TRUE);
	}
	else
	{
		glfwWindowHint(GLFW_STEREO, GL_FALSE);
	}

	// create display context
	window = glfwCreateWindow(w, h, "CHAI3D", NULL, NULL);
	if (!window)
	{
		cout << "failed to create window" << endl;
		cSleepMs(1000);
		glfwTerminate();
		return 1;
	}

	// get width and height of window
	glfwGetWindowSize(window, &width, &height);

	// set position of window
	glfwSetWindowPos(window, x, y);

	// set key callback
	glfwSetKeyCallback(window, keyCallback);

	// set mouse position callback
	glfwSetCursorPosCallback(window, mouseMotionCallback);

  // set mouse button callback
  glfwSetMouseButtonCallback(window, mouseButtonCallback);

	// set resize callback
	glfwSetWindowSizeCallback(window, windowSizeCallback);

	// set current display context
	glfwMakeContextCurrent(window);

	// sets the swap interval for the current display context
	glfwSwapInterval(swapInterval);

#ifdef GLEW_VERSION
	// initialize GLEW library
	if (glewInit() != GLEW_OK)
	{
		cout << "failed to initialize GLEW library" << endl;
		glfwTerminate();
		return 1;
	}
#endif

	//--------------------------------------------------------------------------
	// WORLD - CAMERA - LIGHTING
	//--------------------------------------------------------------------------

	// create a new world.
	world = new cWorld();

	// set the background color of the environment
	world->m_backgroundColor.setWhite();

	// create a camera and insert it into the virtual world
	camera = new cCamera(world);
	world->addChild(camera);

	// position and orient the camera
	cVector3d camerapos(0.2, 0.0, 0.2);
	cVector3d lookat(0.00, 0.0, 0.00);
	cVector3d up(0.0, 0.0, 1.0);
	camera->set(camerapos, // camera position (eye)
				lookat,	// lookat position (target)
				up);	   // direction of the (up) vector

	// set the near and far clipping planes of the camera
	// anything in front or behind these clipping planes will not be rendered
	camera->setClippingPlanes(0.01, 10.0);

	// set stereo mode
	camera->setStereoMode(stereoMode);

	// set stereo eye separation and focal length (applies only if stereo is enabled)
	camera->setStereoEyeSeparation(0.03);
	camera->setStereoFocalLength(1.8);

	// set vertical mirrored display mode
	camera->setMirrorVertical(false);

	// create a light source
	light = new cSpotLight(world);

	// attach light to camera
	world->addChild(light);

	// enable light source
	light->setEnabled(true);

	// position the light source
	light->setLocalPos(0.0, 0.3, 0.4);

	// define the direction of the light beam
	light->setDir(0.0, -0.25, -0.4);

	// enable this light source to generate shadows
	light->setShadowMapEnabled(false);

	// set the resolution of the shadow map
	//light->m_shadowMap->setQualityLow();
	light->m_shadowMap->setQualityHigh();

	// set shadow factor
	world->setShadowIntensity(0.3);

	// set light cone half angle
	light->setCutOffAngleDeg(30);

	//--------------------------------------------------------------------------
	// HAPTIC DEVICES / TOOLS
	//--------------------------------------------------------------------------

	// create a haptic device handler
	handler = new cHapticDeviceHandler();

	// get access to the first available haptic device
	handler->getDevice(hapticDevice, 0);

	// retrieve information about the current haptic device
	cHapticDeviceInfo hapticDeviceInfo = hapticDevice->getSpecifications();

	// retrieve the highest stiffness this device can render
	hapticDeviceMaxStiffness = hapticDeviceInfo.m_maxLinearStiffness;

	// if the haptic devices carries a gripper, enable it to behave like a user switch
	hapticDevice->setEnableGripperUserSwitch(true);

	//--------------------------------------------------------------------------
	// CREATE SPHERES
	//--------------------------------------------------------------------------

	// create texture
	cTexture2dPtr texture = cTexture2d::create();

	// load texture file
	bool fileload = texture->loadFromFile(RESOURCE_PATH("../resources/images/spheremap-3.jpg"));
	if (!fileload)
	{
#if defined(_MSVC)
		fileload = texture->loadFromFile("../resources/images/spheremap-3.jpg");
#endif
	}
	if (!fileload)
	{
		cout << "Error - Texture image failed to load correctly." << endl;
		close();
		return (-1);
	}

	// set NUM_SPHERES to input; if none or negative, default is five
	int NUM_SPHERES = argc > 1 ? atoi(argv[1]) : 5;

	for (int i = 0; i < NUM_SPHERES; i++)
	{
		// create a sphere and define its radius
		Atom *sphere = new Atom(SPHERE_RADIUS);

		// store pointer to sphere primitive
		//spheres[i] = sphere;
		spheres.push_back(sphere);

		// add sphere primitive to world
		world->addChild(sphere);

		//add line to world
		world->addChild(sphere->getVelVector());

		// set the position of the object at the center of the world

		if (i != 0)
		{
			sphere->setInitialPosition();
		}

		// set graphic properties of sphere
		sphere->setTexture(texture);
		sphere->m_texture->setSphericalMappingEnabled(true);
		sphere->setUseTexture(true);

		// Set the first and second sphere (the one being controlled to red initially and the anchor in blue)
		if (i == 0) 	// sphere is current
		{
			sphere->setCurrent(true);
		}
		else if (i == 1)   //sphere is anchor
		{
			sphere->setAnchor(true);
		}
	}

	//debugging
	/*
	for (int i = 0; i < NUM_SPHERES; i++)
	{
		cVector3d posA = spheres[i]->getLocalPos();
		for (int j = 0; j < NUM_SPHERES; j++)
		{
			if (i != j)
			{
				cVector3d posB = spheres[j]->getLocalPos();
				double distancex = (cDistance(posA, posB)) / .02;
				//cout << i << " and " << j << " distance " << distancex << endl;
			}
		}
	}
	*/
	//--------------------------------------------------------------------------
	// WIDGETS
	//--------------------------------------------------------------------------

	// create a font
	font = NEW_CFONTCALIBRI20();

	// create a label to display the haptic and graphic rate of the simulation
	labelRates = new cLabel(font);
	labelRates->m_fontColor.setBlack();
	camera->m_frontLayer->addChild(labelRates);

	//potential energy label
	LJ_num = new cLabel(font);
	LJ_num->m_fontColor.setBlack();
	camera->m_frontLayer->addChild(LJ_num);

	//total energy label
	total_energy = new cLabel(font);
	total_energy->m_fontColor.setBlack();
	camera->m_frontLayer->addChild(total_energy);

	// create a background
	background = new cBackground();
	camera->m_backLayer->addChild(background);

	// set aspect ration of background image a constant
	background->setFixedAspectRatio(true);

	// load background image
	fileload = background->loadFromFile(RESOURCE_PATH("../resources/images/background.png"));
	if (!fileload)
	{
#if defined(_MSVC)
		fileload = background->loadFromFile("../resources/images/background.png");
#endif
	}
	if (!fileload)
	{
		cout << "Error - Image failed to load correctly." << endl;
		close();
		return (-1);
	}

	//create a scope to plot potential energy

	scope = new cScope();
	scope->setLocalPos(0, 60);
	camera->m_frontLayer->addChild(scope);
	scope->setSignalEnabled(true, true, false, false);
	scope->setTransparencyLevel(.7);
	global_minimum = getGlobalMinima(spheres.size());
	double lower_bound, upper_bound;
	if (global_minimum > -50) {
		upper_bound = 0;
		lower_bound = global_minimum - .5;
	} else {
		upper_bound = 0 + (global_minimum * .2);
		lower_bound = global_minimum - 3;
	}
	scope->setRange(lower_bound, upper_bound);

	//--------------------------------------------------------------------------
	// START SIMULATION
	//--------------------------------------------------------------------------

	// create a thread which starts the main haptics rendering loop
	hapticsThread = new cThread();
	hapticsThread->start(updateHaptics, CTHREAD_PRIORITY_HAPTICS);

	// setup callback when application exits
	atexit(close);

	//--------------------------------------------------------------------------
	// MAIN GRAPHIC LOOP
	//--------------------------------------------------------------------------

	// call window size callback at initialization
	windowSizeCallback(window, width, height);

	// main graphic loop
	while (!glfwWindowShouldClose(window))
	{
		// get width and height of window
		glfwGetWindowSize(window, &width, &height);

		// render graphics
		updateGraphics();

		// swap buffers
		glfwSwapBuffers(window);

		// process events
		glfwPollEvents();

		// signal frequency counter
		freqCounterGraphics.signal(1);
	}

	// close window
	glfwDestroyWindow(window);

	// terminate GLFW library
	glfwTerminate();

	// exit
	return 0;
}

//------------------------------------------------------------------------------

void windowSizeCallback(GLFWwindow *a_window, int a_width, int a_height)
{
	// update window size
	width = a_width;
	height = a_height;

	// update position of message label
	//labelMessage->setLocalPos((int)(0.5 * (width - labelMessage->getWidth())), 50);
}

//------------------------------------------------------------------------------

void errorCallback(int a_error, const char *a_description)
{
	cout << "Error: " << a_description << endl;
}

//------------------------------------------------------------------------------

void keyCallback(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods)
{
	// filter calls that only include a key press
	if ((a_action != GLFW_PRESS) && (a_action != GLFW_REPEAT))
	{
		return;
	}

	// option - exit
	else if ((a_key == GLFW_KEY_ESCAPE) || (a_key == GLFW_KEY_Q))
	{
		glfwSetWindowShouldClose(a_window, GLFW_TRUE);
	}

	// option - toggle fullscreen
	else if (a_key == GLFW_KEY_F)
	{
		// toggle state variable
		fullscreen = !fullscreen;

		// get handle to monitor
		GLFWmonitor *monitor = glfwGetPrimaryMonitor();

		// get information about monitor
		const GLFWvidmode *mode = glfwGetVideoMode(monitor);

		// set fullscreen or window mode
		if (fullscreen)
		{
			glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			glfwSwapInterval(swapInterval);
		}
		else
		{
			int w = 2. * mode->height;
			int h = 1.5 * mode->height;
			int x = 1.5 * (mode->width - w);
			int y = 1.5 * (mode->height - h);
			glfwSetWindowMonitor(window, NULL, x, y, w, h, mode->refreshRate);
			glfwSwapInterval(swapInterval);
		}
	}
	// action - unanchor all key
	else if (a_key == GLFW_KEY_U) {
		for (auto i {0}; i < spheres.size(); i++) {
			if (spheres[i]->isAnchor()) {
				spheres[i]->setAnchor(false);
			}
		}
	}
  // option - save screenshot to file
  else if (a_key == GLFW_KEY_S)
  {
    cImagePtr image = cImage::create();
    camera->m_frontLayer->removeChild(scope);
    camera->renderView(width, height);
    camera->copyImageBuffer(image);
    camera->m_frontLayer->addChild(scope);
    image->saveToFile("atoms.png");
  }
}

void mouseButtonCallback(GLFWwindow* a_window, int a_button, int a_action, int a_mods)
{
		// store mouse position
		double x,y;

		// detect for any collision between mouse and scene
		cCollisionRecorder recorder;
		cCollisionSettings settings;

    if (a_button == GLFW_MOUSE_BUTTON_LEFT && a_action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &x, &y);
        bool hit = camera->selectWorld(x, (height-y), width, height, recorder, settings);
        if (hit)
        {
						cGenericObject *selected = recorder.m_nearestCollision.m_object;
						selectedAtom = (Atom*) selected;
						selectedPoint = recorder.m_nearestCollision.m_globalPos;
						selectedAtomOffset = recorder.m_nearestCollision.m_globalPos - selectedAtom->getLocalPos();
						mouseState = MOUSE_SELECTION;
				}
			}else if(a_button == GLFW_MOUSE_BUTTON_RIGHT && a_action == GLFW_PRESS){
				glfwGetCursorPos(window, &x, &y);
				bool hit = camera->selectWorld(x, (height-y), width, height, recorder, settings);
				if (hit)
				{
					// retrieve Atom selected by mouse
					cGenericObject *selected = recorder.m_nearestCollision.m_object;
					selectedAtom = (Atom*) selected;

					// Toggle anchor status and color
					if (selectedAtom->isAnchor()) {
						selectedAtom->setAnchor(false);
					} else if(!selectedAtom->isCurrent()){     //cannot set current to anchor
						selectedAtom->setAnchor(true);
					}
					mouseState = MOUSE_SELECTION;
				}
			}else{
				mouseState = MOUSE_IDLE;
			}
}
//------------------------------------------------------------------------------

void close(void)
{
	// stop the simulation
	simulationRunning = false;

	// wait for graphics and haptics loops to terminate
	while (!simulationFinished)
	{
		cSleepMs(100);
	}

	// delete resources
	delete hapticsThread;
	delete world;
	delete handler;
}

//------------------------------------------------------------------------------

void updateGraphics(void)
{
	/////////////////////////////////////////////////////////////////////
	// UPDATE WIDGETS
	/////////////////////////////////////////////////////////////////////

	// update haptic and graphic rate data
	labelRates->setText(cStr(freqCounterGraphics.getFrequency(), 0) + " Hz / " +
						cStr(freqCounterHaptics.getFrequency(), 0) + " Hz");

	// update position of label
	labelRates->setLocalPos((int)(0.5 * (width - labelRates->getWidth())), 15);

	/////////////////////////////////////////////////////////////////////
	// RENDER SCENE
	/////////////////////////////////////////////////////////////////////

	// update shadow maps (if any)
	world->updateShadowMaps(false, false);

	// render world
	camera->renderView(width, height);
	// wait until all GL commands are completed
	glFinish();

	// check for any OpenGL errors
	GLenum err = glGetError();
	if (err != GL_NO_ERROR)
		cout << "Error: " << gluErrorString(err) << endl;
}

//------------------------------------------------------------------------------

void updateHaptics(void)
{
	Atom *current;
	Atom *previous;

	// simulation in now running
	simulationRunning = true;
	simulationFinished = false;

	// a flag to indicate if haptic forces are active
	bool flagHapticsEnabled = false;

	// reset clock
	cPrecisionClock clock;
	clock.reset();

	// open a connection to haptic device
	hapticDevice->open();

	// calibrate device (if necessary)
	hapticDevice->calibrate();

	// Track which atom is currently being moved
	int curr_atom = 0;
	int anchor_atom = 1;
	int anchor_atom_hold = 1;

	//Array for all velocities
	//cVector3d sphereVel[NUM_SPHERES];
	//cout << sphereVel[NUM_SPHERES - 1] << endl;
	// main haptic simulation loop

	bool button1_changed = false;
	bool button2_changed = false;
	bool button3_changed = false;
	bool is_anchor = true;
	while (simulationRunning)
	{
		/////////////////////////////////////////////////////////////////////
		// SIMULATION TIME
		/////////////////////////////////////////////////////////////////////

		// stop the simulation clock
		clock.stop();

		// read the time increment in seconds
		double timeInterval = cMin(0.001, clock.getCurrentTimeSeconds());

		// restart the simulation clock
		//clock.reset();
		clock.start();

		// signal frequency counter
		freqCounterHaptics.signal(1);

		/////////////////////////////////////////////////////////////////////////
		// READ HAPTIC DEVICE
		/////////////////////////////////////////////////////////////////////////

		// read position
		cVector3d position;
		hapticDevice->getPosition(position);
		//Scale position to use more of the screen
		//increase to use more of the screen
		position *= 2.0;
		// read user-switch status (button 0)

		/////////////////////////////////////////////////////////////////////////
		// UPDATE SIMULATION
		/////////////////////////////////////////////////////////////////////////

		// position of walls and ground
		const double WALL_GROUND = 0.0 + SPHERE_RADIUS;
		const double WALL_CEILING = 0.05; //0.2;
		const double WALL_LEFT = -0.05;   //-0.1;
		const double WALL_RIGHT = 0.05;   //0.2;
		const double WALL_FRONT = 0.05;   //0.08;
		const double WALL_BACK = -0.05;   //-0.08;
		const double SPHERE_STIFFNESS = 500.0;
		const double SPHERE_MASS = 0.02;
		const double K_DAMPING = 0.001; //0.996;
		const double K_MAGNET = 500.0;
		const double HAPTIC_STIFFNESS = 1000.0;
		const double SIGMA = 1.0;
		const double EPSILON = 1.0;
		const double FORCE_DAMPING = .75;
		//Scales the distance betweens atoms
		const double DIST_SCALE = .02;

		//Update current atom based on if the user pressed the far left button
		//The point of button2_changed is to make it so that it only switches one atom if the button is touched Otherwise it flips out

		bool button0;
		hapticDevice->getUserSwitch(0, button0);
		bool button1;
		hapticDevice->getUserSwitch(1, button1);
		bool button2;
		hapticDevice->getUserSwitch(2, button2);
		bool button3;
		hapticDevice->getUserSwitch(3, button3);


		// Changes the camera when button2 is pressed
		if (button2)
		{
			if (!button2_changed)
			{
				//rotates camera around in square
				switch (curr_camera)
				{
				case 1:
					camera->set(cVector3d(.2, 0.0, -.2), cVector3d(0.0, 0.0, 0.0), cVector3d(0.0, 0.0, 0.1));
					break;
				case 2:
					camera->set(cVector3d(-.2, 0.0, .2), cVector3d(0.0, 0.0, 0.0), cVector3d(0.0, 0.0, 0.1));
					break;
				case 3:
					camera->set(cVector3d(-.2, 0.0, -.2), cVector3d(0.0, 0.0, 0.0), cVector3d(0.0, 0.0, 0.1));
					break;
				case 4:
					camera->set(cVector3d(.2, 0.0, .2), cVector3d(0.0, 0.0, 0.0), cVector3d(0.0, 0.0, 0.1));
					curr_camera = 0;
					break;
				}
				curr_camera++;
				button2_changed = true;
			}
		}
		else
			button2_changed = false;

		// Changes the current atom being controlled when button 1 is pressed
		// JD: edit so that we use remainder function in C++ and remove previous if else statement
		if (button1)
		{
			if (!button1_changed)
			{
				// computes current atom by taking the remainder of the curr_atom +1 and number of spheres
				int previous_curr_atom = curr_atom;
				curr_atom = remainder(curr_atom + 1, spheres.size());
				if (curr_atom < 0)
				{
					curr_atom = spheres.size() + curr_atom;
				}

				// Skip anchored atoms; will eventually terminate at previous_curr_atom
				while(spheres[curr_atom]->isAnchor()){
					curr_atom = remainder(curr_atom + 1, spheres.size());
					if (curr_atom < 0)
					{
						curr_atom = spheres.size() + curr_atom;
					}
				}

				current = spheres[curr_atom];
				previous = spheres[previous_curr_atom];

				cVector3d A = current->getLocalPos();

				// Change attributes of previous current and new current
				previous->setCurrent(false);
				previous->setLocalPos(A);
				current->setCurrent(true);
				current->setLocalPos(position);
				cVector3d translate = (previous->getLocalPos()) - (current->getLocalPos());

				Atom *traverser;
				for (int i = 0; i < spheres.size(); i++)
				{
					traverser = spheres[i];
					if (i != curr_atom)
					{
						if (i == (previous_curr_atom))
						{
							traverser->setLocalPos(previous->getLocalPos() - (2.0 * translate));
							cVector3d positions = traverser->getLocalPos();
						}
						else
						{
							traverser->setLocalPos(traverser->getLocalPos() - (translate));
							cVector3d positions = traverser->getLocalPos();
						}
					}
				}
				button1_changed = true;
			}
		}
		else{
			button1_changed = false;
		}

		// JD: added use for button 3; blue atom is now anchor and is fixed in place in the simulation
		// if you want to change the anchor atom press button 3
		// Changes the current anchor when button 3 is pressed
		// When all anchors have been cycled through, there is a setting where there is no anchor.
		if (button3)
		{
			if (!button3_changed)
			{
				bool anchor_changed = true;
				anchor_atom_hold = anchor_atom;
				anchor_atom = remainder(anchor_atom + 1, spheres.size());
				if (anchor_atom < 0)
				{
					anchor_atom = spheres.size() + anchor_atom;
				}
				if (anchor_atom == curr_atom)
				{
					anchor_changed = false;
				}
				button3_changed = true;
				if (anchor_changed)
				{
					if (is_anchor)
					{
						spheres[anchor_atom_hold]->setAnchor(false);
					}
					spheres[anchor_atom]->setAnchor(true);
					is_anchor = true;
				}
				else
				{
					spheres[anchor_atom_hold]->setAnchor(false);
					is_anchor = false;
				}
			}
		}
		else
			button3_changed = false;

		// compute forces for all spheres
		double lj_PE = 0;

		// JD: edited this so that many operations are removed out of the inner loop
		// This loop is for computing the force on atom i
		for (int i = 0; i < spheres.size(); i++)
		{
			// compute force on atom
			cVector3d force;
			current = spheres[i];
			cVector3d pos0 = current->getLocalPos();
			// check forces with all other spheres
			force.zero();

			// this loop is for finding all of atom i's neighbors
			for (int j = 0; j < spheres.size(); j++)
			{
				//Don't compute forces between an atom and itself
				if (i != j)
				{
					// get position of sphere
					cVector3d pos1 = spheres[j]->getLocalPos();

					// compute direction vector from sphere 0 to 1

					cVector3d dir01 = cNormalize(pos0 - pos1);

					// compute distance between both spheres
					double distance = cDistance(pos0, pos1) / DIST_SCALE;

					//cout << "array " << (sizeof(lj_potential)/sizeof(*lj_potential)) << endl;
					double lj_potential[spheres.size()];
					lj_potential[i] = {4 * EPSILON * (pow(SIGMA / distance, 12) - pow(SIGMA / distance, 6))};

					lj_PE = lj_PE + lj_potential[i];
					if (!button0)
					{

						double lj = -4 * FORCE_DAMPING * EPSILON * ((-12 * pow(SIGMA / distance, 13)) - (-6 * pow(SIGMA / distance, 7)));
						force.add(lj * dir01);
					}
				}
			}
			current->setForce(force);
			// update velocity and position of all spheres
			// compute acceleration
			cVector3d sphereAcc = (force / SPHERE_MASS);
			current->setVelocity(K_DAMPING * (current->getVelocity() + timeInterval * sphereAcc));
			// compute /position
			cVector3d spherePos_change = timeInterval * current->getVelocity() + cSqr(timeInterval) * sphereAcc;
			double magnitude = spherePos_change.length();

			cVector3d spherePos = current->getLocalPos() + spherePos_change;
			if (magnitude > 5)
			{
				cout << i << " velocity " << current->getVelocity().length() << endl;
				cout << i << " force " << force.length() << endl;
				cout << i << " acceleration " << sphereAcc.length() << endl;
				cout << i << " time " << timeInterval << endl;
				cout << i << " position of  " << timeInterval << endl;
			}

			// update value to sphere object
			//double kinetic_energy = .5 * SPHERE_MASS * pow(sphereVel[i].length(), 2);

			// update position of label
			//total_energy->setLocalPos(20, 0);

			if (!current->isCurrent())
			{
				if (!current->isAnchor())
				{
					current->setLocalPos(spherePos);
				}
			}
		}
		current = spheres[curr_atom];
		current->setLocalPos(position);
		cVector3d force = current->getForce();
		// JD: moved this out of nested for loop so that test is set only when fully calculated
		// update haptic and graphic rate data
		LJ_num->setText("Potential Energy: " + cStr((lj_PE / 2), 5));

		// update position of label
		LJ_num->setLocalPos(0, 0);

		// Update scope
		double currentTime = clock.getCurrentTimeSeconds();
		// rounds current time to the nearest tenth
		double currentTimeRounded = double(int(currentTime * 10 + .5)) / 10;
		// The number fmod() is compared to is the threshold, this adjusts the timescale
		if (fmod(currentTime, currentTimeRounded) <= .01)
		{
			scope->setSignalValues(lj_PE/2, global_minimum);
		}
		/////////////////////////////////////////////////////////////////////////
		// FORCE VECTOR
		/////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < spheres.size(); i++)
		{
			current = spheres[i];
			cVector3d newPoint = cAdd(current->getLocalPos(), current->getForce());
			cVector3d newPointNormalized;
			current->getForce().normalizer(newPointNormalized);
			current->getVelVector()->m_pointA = cAdd(current->getLocalPos(), newPointNormalized * current->getRadius());
			current->getVelVector()->m_pointB = cAdd(current->getVelVector()->m_pointA, current->getForce() * .005);
			current->getVelVector()->setLineWidth(5);

			// Change color, red if current, black otherwise
			if (i == curr_atom)
			{
				current->getVelVector()->m_colorPointA.setRed();
				current->getVelVector()->m_colorPointB.setRed();
			}
			else
			{
				current->getVelVector()->m_colorPointA.setBlack();
				current->getVelVector()->m_colorPointB.setBlack();
			}

			// TODO - experiment with threshold
			//float dist = velVectors[i]->m_pointA.distance(velVectors[i]->m_pointB);
			//if (dist >= .05 ) {
			//    velVectors[i]->m_pointB = cAdd(velVectors[i]->m_pointA, newPointNormalized * .05);
			//}
		}

		/////////////////////////////////////////////////////////////////////////
		// APPLY FORCES
		/////////////////////////////////////////////////////////////////////////

		// scale the force according to the max stiffness the device can render
		double stiffnessRatio = 1.0;
		if (hapticDeviceMaxStiffness < HAPTIC_STIFFNESS)
			stiffnessRatio = hapticDeviceMaxStiffness / HAPTIC_STIFFNESS;
		if (force.length() > 10)
			force = 10. * cNormalize(force);
		// send computed force to haptic device
		hapticDevice->setForce(stiffnessRatio * force);
	}

	// close  connection to haptic device
	hapticDevice->close();

	// exit haptics thread
	simulationFinished = true;
}

double getGlobalMinima(int cluster_size) {
	string file_path = "../resources/data/";
	string file_name = "global_minima.txt";
	ifstream infile(file_path + file_name);

	if (!infile) {
		cerr << "Could not open \"" + file_name + "\" for reading" << endl;
		cerr << "Did you move it to \"" + file_path+ "\"?" << endl;
		exit(1);
	} else if ((cluster_size < 2) || (cluster_size > 150)) {
        cout << "WARNING \"" + file_name + "\" doesn't have data for clusters of this size yet." << endl;
		cout << "The graph may not be accurate" << endl;
	}

	int cluster_size_file;
	double minimum;
	while(infile >> cluster_size_file >> minimum) {
		if (cluster_size_file == cluster_size) {
			break;
		}
	}
	cout << minimum << endl;
	return minimum;
}

void mouseMotionCallback(GLFWwindow* a_window, double a_posX, double a_posY)
{
    if ((selectedAtom != NULL) && (mouseState == MOUSE_SELECTION) && (selectedAtom->isAnchor()))
    {
        // get the vector that goes from the camera to the selected point (mouse click)
        cVector3d vCameraObject = selectedPoint - camera->getLocalPos();

        // get the vector that point in the direction of the camera. ("where the camera is looking at")
        cVector3d vCameraLookAt = camera->getLookVector();

        // compute the angle between both vectors
        double angle = cAngle(vCameraObject, vCameraLookAt);

        // compute the distance between the camera and the plane that intersects the object and
        // which is parallel to the camera plane
        double distanceToObjectPlane = vCameraObject.length() * cos(angle);

        // convert the pixel in mouse space into a relative position in the world
        double factor = (distanceToObjectPlane * tan(0.5 * camera->getFieldViewAngleRad())) / (0.5 * height);
        double posRelX = factor * (a_posX - (0.5 * width));
        double posRelY = factor * ((height - a_posY) - (0.5 * height));

        // compute the new position in world coordinates
        cVector3d pos = camera->getLocalPos() +
            distanceToObjectPlane * camera->getLookVector() +
            posRelX * camera->getRightVector() +
            posRelY * camera->getUpVector();

        // compute position of object by taking in account offset
        cVector3d posObject = pos - selectedAtomOffset;

        // apply new position to object
        selectedAtom->setLocalPos(posObject);

    }
}