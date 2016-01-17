//Author: Caleb Search
//Date: 1/13/2016

// Controls a stepper motor via an LCD, 4 Buttons, and 2 limit switches.

#include <LiquidCrystal.h>

/* -- Constants -- */
#define SYRINGE_VOLUME_ML 10.0
#define SYRINGE_BARREL_LENGTH_MM 10.0

#define THREADED_ROD_PITCH 1.25
#define STEPS_PER_REVOLUTION 200.0
#define MICROSTEPS_PER_STEP 8.0

float ustepsPerMM = MICROSTEPS_PER_STEP * STEPS_PER_REVOLUTION / THREADED_ROD_PITCH;
float ustepsPerML = (MICROSTEPS_PER_STEP * STEPS_PER_REVOLUTION * SYRINGE_BARREL_LENGTH_MM) / (SYRINGE_VOLUME_ML * THREADED_ROD_PITCH);

/* -- Pin definitions -- */
int motorDirPin = 4;
int motorStepPin = 5;
int motorEnablePin = 13;

int endLimitPin = 1;
int homeLimitPin = 2;

int limitInterruptPin = 2;
int buttonInterruptPin = 3;

int startStopButton = A5;
int homeButton = A4;
int upButton = A3;
int downButton = A2;
int greenLED = A1;
int redLED = A0;

/* -- keys --*/
enum { KEY_START, KEY_UP, KEY_DOWN, KEY_HOME, KEY_NONE };
int NUM_KEYS = 4;
int adc_key_in;
int key = KEY_NONE;

/* -- Enums and constants -- */
enum { PUSH, PULL }; //syringe movement direction
enum { MAIN, VOLUME, SPEED }; //UI states

/* -- Default Parameters -- */
float mLUsed = 0.0;
float mlVolume = 0.5;
float pumpSpeedmlPM = 0.500;
long stepDelay = 100;
long lastStepTime = 0;
bool pause = true;

float stepperPos = 0; //in microsteps
float maxStepperPos = 0;
char charBuf[16];

//debounce params
long lastKeyRepeatAt = 0;
long keyRepeatDelay = 400;
long keyDebounce = 125;
int prevKey = KEY_NONE;

//menu stuff
int uiState;

/* -- Initialize libraries -- */
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

void setup()
{
	/* LCD setup */
	lcd.begin(20, 4);
	lcd.clear();

	lcd.setCursor(0, 0); //set the LCD cursor to 0,0
	lcd.print("SyringePump v2.0");
	lcd.setCursor(0, 1);
	lcd.print("By Caleb Search");
	delay(1000);

   /* Triggering setup */
	pinMode(upButton, INPUT);
	pinMode(downButton, INPUT);
	pinMode(startStopButton, INPUT);
	pinMode(homeButton, INPUT);
	pinMode(endLimitPin, INPUT);
	pinMode(homeLimitPin, INPUT);

	/* Motor Setup */
	pinMode(motorEnablePin, OUTPUT);
	//default disable motor
	motorDisable();
	pinMode(motorDirPin, OUTPUT);
	pinMode(motorStepPin, OUTPUT);

	//get the speed to pump
	getSpeed();

	//home
	//home();

	//interrupt
	attachInterrupt(digitalPinToInterrupt(3), readKey, RISING);
	attachInterrupt(digitalPinToInterrupt(2), hitLimit, RISING);
}

void loop()
{

	uiState = MAIN;
	//wait for keys to be pressed

	//pause the pump
	pauseCheck();

	mLUsed = stepperPos / ustepsPerML;

	//print to LCD
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Injected: ");
	lcd.print(mLUsed, 4);
	lcd.print(" ml");
	lcd.setCursor(0, 1);
	lcd.print("Time: ");
	lcd.print(((maxStepperPos - stepperPos) / ustepsPerML / pumpSpeedmlPM), 2);
	lcd.print(" min");
	lcd.setCursor(0, 2);
	lcd.print("Speed: ");
	lcd.print(pumpSpeedmlPM, 3);
	lcd.print(" ml/min");

	//pump by increments of 20 steps
	if (stepperPos < maxStepperPos)
	{
		step(20);
		stepperPos += 20;
	}
	else
	{
		//do nothing because it's finished
		motorDisable();
		detachInterrupt(digitalPinToInterrupt(3));
		detachInterrupt(digitalPinToInterrupt(2));
		while (true)
		{
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Pumping Finished");
			lcd.setCursor(0, 1);
			lcd.print("Please Restart");
			delay(1000);
		}
	}
}

//pause check for the pump
void pauseCheck()
{
	//enable interrupts again
	
	while (true)
	{
		readKey();
		if (pause == false)
		{
			motorEnable();
			break;
		}
		//disable the motor
		motorDisable();
		//display paused
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Paused");
		lcd.setCursor(0, 1);
		lcd.print("Press start");
		lcd.setCursor(0, 2);
		lcd.print("to resume");
		delay(100);
	}
}

/*
* Sets the speed in steps per minute
*/
void setSpeed(float whatSpeed)
{
	stepDelay = 60 * 1000000 / whatSpeed;
}

/*
* Moves the motor steps_to_move steps.  If the number is negative,
* the motor moves in the reverse direction.
*/
void step(int stepsToMove)
{
	int stepsLeft = abs(stepsToMove);  // how many steps to take

	// determine direction based on whether steps_to_mode is + or -:
	if (stepsToMove > 0) { digitalWrite(motorDirPin, HIGH); }
	if (stepsToMove < 0) { digitalWrite(motorDirPin, LOW); }


	// decrement the number of steps, moving one step each time:
	while (stepsLeft > 0)
	{
		unsigned long now = micros();
		// move only if the appropriate delay has passed:
		if (now - lastStepTime >= stepDelay)
		{
			// get the timeStamp of when you stepped:
			lastStepTime = now;
			// decrement the steps left:
			stepsLeft--;
			// step the motor to step number 0, 1, ..., {3 or 10}
			digitalWrite(motorStepPin, HIGH);
			delayMicroseconds(5);
			digitalWrite(motorStepPin, LOW);
		}
	}
}

//get the speed to pump from the user
void getSpeed()
{
	uiState = SPEED; //interface is in set Speed section

	//print to the LCD
	lcd.clear();
	lcd.setCursor(0,0);						//first row
	lcd.print("Select a speed:");
	lcd.setCursor(0, 1);					//second row
	lcd.print(pumpSpeedmlPM, 3);
	lcd.print(" ml/min");
	lcd.setCursor(0, 2);					//third row
	lcd.print("1.0 - 0.005 ml/min");

	//get the pumpSpeed from the user
	while (digitalRead(startStopButton) == LOW) //should improve this with a better end case
	{
		//check for key presses
		readKey();
		//update the LCD
		lcd.setCursor(0, 1);
		lcd.print(pumpSpeedmlPM, 3);
		lcd.print(" ml/min");
	}

	//delay to be sure of no double presses
	delay(200); //100 milliseconds or 0.1 second

	//change the ui state to volume input
	uiState = VOLUME;
	
	//print to the LCD
	lcd.clear();
	lcd.setCursor(0, 0);						//first row
	lcd.print("Select a volume:");
	lcd.setCursor(0, 1);					//second row
	lcd.print(mlVolume, 3);
	lcd.print(" ml");
	lcd.setCursor(0, 2);					//third row
	lcd.print("1000.0 - 0.01 ml");

	//get the volume from the user
	while (digitalRead(startStopButton) == LOW) //should improve this with a better end case
	{
		//check for key presses
		readKey();
		//update the LCD
		lcd.setCursor(0, 1);
		lcd.print(mlVolume, 3);
		lcd.print(" ml");
	}

	//set the maximum stepper position
	maxStepperPos = mlVolume*ustepsPerML;
}

//enable the motor
void motorEnable()
{
	digitalWrite(motorEnablePin, HIGH);
}

//diable the motor
void motorDisable()
{
	digitalWrite(motorEnablePin, LOW);
}

//home the axis
void home()
{

	//enable the motor
	motorEnable();
	//home the axis
	while (digitalRead(homeLimitPin) == LOW)
	{
		//reverse the stepper
		step(-20);
		//set the variables to 0
		stepperPos = 0;
		mLUsed = 0;
	}
	//disable the motor again
	motorDisable();
}

//check to see which button is being pressed
int checkButton()
{
	//start/stop button
	if (digitalRead(startStopButton) == HIGH && digitalRead(upButton) == LOW && digitalRead(downButton) == LOW && digitalRead(homeButton) == LOW)
	{
		//lcd.setCursor(0, 3);
		//lcd.print("Start");
		//delay(100);
		return KEY_START;
	}
	//home button
	else if (digitalRead(startStopButton) == LOW && digitalRead(upButton) == LOW && digitalRead(downButton) == LOW && digitalRead(homeButton) == HIGH)
	{
		//lcd.setCursor(0, 3);
		//lcd.print("Home");
		//delay(100);
		return KEY_HOME;
	}
	//up button
	else if (digitalRead(startStopButton) == LOW && digitalRead(upButton) == HIGH && digitalRead(downButton) == LOW && digitalRead(homeButton) == LOW)
	{
		//lcd.setCursor(0, 3);
		//lcd.print("Up");
		//delay(100);
		return KEY_UP;
	}
	//down button
	else if (digitalRead(startStopButton) == LOW && digitalRead(upButton) == LOW && digitalRead(downButton) == HIGH && digitalRead(homeButton) == LOW)
	{
		//lcd.setCursor(0, 3);
		//lcd.print("Down");
		//delay(100);
		return KEY_DOWN;
	}
	//false trigger
	else
	{
		//lcd.setCursor(0, 3);
		//lcd.print("None");
		//delay(100);
		return KEY_NONE;
	}
}

//trigger for the limit switches
void hitLimit()
{
	//disable the motor
	motorDisable();
	//if end switch is hit
	if (digitalRead(limitInterruptPin) == HIGH && digitalRead(homeLimitPin) == LOW)
	{
		//display finished message
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Endstop Hit");
		lcd.setCursor(0, 1);
		lcd.print("Remove the syringe");
		lcd.setCursor(0, 2);
		lcd.print("then hit home");
		//wait for home to be hit
		while (true)
		{
			//infinate loop for now
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Endstop Hit");
			lcd.setCursor(0, 1);
			lcd.print("Please Restart");
			delay(1000);
		}
	}
	else
	{
		//if home switch
		//display home hit
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Endstop Hit");
		lcd.setCursor(0, 1);
		lcd.print("Insert the syringe");
		lcd.setCursor(0, 2);
		lcd.print("then hit start");
		//ask to press button to start and wait for start
		pauseCheck();
	}
}

//trigger for the buttons
void readKey() {
	//Some UI niceness here. 
	//When user holds down a key, it will repeat every so often (keyRepeatDelay).
	//But when user presses and releases a key, 
	//the key becomes responsive again after the shorter debounce period (keyDebounce).

	//disable interrupts while key stuff is going on
	//detachInterrupt(digitalPinToInterrupt(buttonInterruptPin));

	key = checkButton(); //get which button was pressed

	long currentTime = millis();
	long timeSinceLastPress = (currentTime - lastKeyRepeatAt);

	boolean processThisKey = false;
	if (prevKey == key && timeSinceLastPress > keyRepeatDelay) {
		processThisKey = true;
	}
	if (prevKey == KEY_NONE && timeSinceLastPress > keyDebounce) {
		processThisKey = true;
	}
	if (key == KEY_NONE) {
		processThisKey = false;
	}

	prevKey = key;

	if (processThisKey) {
		doKeyAction(key);
		lastKeyRepeatAt = currentTime;
	}
}

void doKeyAction(unsigned int key) {
	if (key == KEY_NONE)
	{
		//do nothing
	}

	if (uiState == MAIN)
	{
		if (key == KEY_UP)
		{
			//main menu code here button up
			pumpSpeedmlPM += 0.005;
		}
		else if (key == KEY_DOWN)
		{
			//main menu code here button down
			pumpSpeedmlPM -= 0.005;
		}
		else if (key == KEY_START)
		{
			//toggle pause on the pump
			if (pause)
			{
				pause = false;
			}
			else
			{
				pause = true;
			}
		}
		else if (key == KEY_HOME)
		{
			//pause the motor
			pause = true;
			//make sure they want to home
			lcd.clear();
			lcd.setCursor(0, 0);
			lcd.print("Are you sure you");
			lcd.setCursor(0, 1);
			lcd.print("want to home?");
			lcd.setCursor(0, 2);
			lcd.print("start to cancel");

			//wait for a second button press
			while (digitalRead(homeButton) == HIGH || digitalRead(startStopButton) == HIGH)
			{
				//do nothing
			}
			while (true)
			{
				//check to see what button was hit
				if (digitalRead(startStopButton) == HIGH && digitalRead(homeButton) == LOW)
				{
					pause = false;
				}
				else if (digitalRead(startStopButton) == LOW && digitalRead(homeButton) == HIGH)
				{
					//home axis
					home();
				}
			}
		}
	}
	else if (uiState == SPEED)
	{
		if (key == KEY_UP)
		{
			//do up key code here
			pumpSpeedmlPM += 0.005;
			if (pumpSpeedmlPM > 1) { pumpSpeedmlPM = 1; }
		}
		if (key == KEY_DOWN)
		{
			//down down key code here
			pumpSpeedmlPM -= 0.005;
			if (pumpSpeedmlPM < 0) { pumpSpeedmlPM = 0; }

		}
	}
	else if (uiState == VOLUME)
	{
		if (key == KEY_UP)
		{
			//do up key code here
			mlVolume += 0.01;
			if (mlVolume > 1000) { mlVolume = 1; }
		}
		if (key == KEY_DOWN)
		{
			//down down key code here
			mlVolume -= 0.01;
			if (mlVolume < 0) { mlVolume = 0; }

		}
	}
	//process the giver pump speed into steps/min and set it
	setSpeed(pumpSpeedmlPM*ustepsPerML);
}