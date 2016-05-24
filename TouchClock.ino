
/****************************************************/
/*    // Stuarts code to drive TFT display          */
/*                          */
/* 2.4 inch touch shield                          */
/* Shows Time and allows adjustment                         */
/* Shows a list of stored times for recall               */
/* Allows fast forward and rewind functions             */
/*                          */
/*                          */

/****************************************************/

// Display 
// menu buttons
// list area
// edit overlay
// slider bar

// include the library code:

#include <Arduino.h>

#include <Adafruit_GFX.h>    // Core graphics library
#include <SWTFT.h> // Hardware-specific library
#include <TouchScreen.h>

#include <Time.h>
//#include <TimeLib.h>
//#include <Wire.h>
//#include <DS1307RTC.h>  // RTC

#include <String.h>

// watchdog
//#include <avr/wdt.h>

#define DEBUGWATCHDOG false
#define DEBUGREPORT true
#define DEBUGTIME false
#define DEBUGTOUCH false
#define DEBUGBUTTONS true
#define DEBUGCLOCK true

#define YP A1  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 7   // can be a digital pin
#define XP 6   // can be a digital pin

// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define MINPRESSURE 10
#define MAXPRESSURE 1000

#define BOXSIZE 40
#define PENRADIUS 3
#define LISTSIZE 20

// ************************************
// Define Objects and variables
// ************************************

SWTFT tft;
tmElements_t tm;
time_t timeNow;
// date storage data type
struct stuTime{
	int millenia; // millenea offset
	int year;
	int month;
	int day;
	int hour;
	int minute;
  int second;
} ;

// bank of stored dates

stuTime myDates[LISTSIZE];  // array of dates
int listPtr=0; // pointer to current date
int clockPtr=0;  // which one is the clock written to
int displayPtr=0; // which one is displayed on the clockface
stuTime offsetTime = {0,0,0,0,0,0,0};  // display time offset - allows FF and rewind

// resistance between X+ and X-  300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

int lastsecond=-1; // holds the prevoius second

// screen calibration
int TS_MINX = 220;
int TS_MINY =180;
int TS_MAXX =850;
int TS_MAXY =900;
int xoffset=10;
int yoffset=-12;

// int i2cdelay=5;//wait after each packet
int oldcolor, currentcolor;
TSPoint touched; // last valid point
TSPoint buttonSize; // dimensions of button x,y
// Declare all the global vars
//unsigned long waitcheckTime=0; // timer for time checking
//unsigned long waitcheckButtons=0; // timer for buttons
//unsigned long intervalcheckTime=1012;
//unsigned long intervalcheckButtons=503;
unsigned long reportTime=1007;
unsigned long reportInterval=10007;
//unsigned long lasttime=0;
//unsigned long thistime=0; // Time handles
unsigned long millisNow=0; // used for checking internal loop timers
// unsigned long NextTimeSyncTime=0;
// unsigned long NextTimeSyncInterval=60023; // 30 seconds
int screenWidth =0;
int screenHeight = 0;
int screenRotation = 0;
int buttonCount=6;
int menuState=0; // which menu we're in

const char *monthName[12] = {// array of month names
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

const int daysInMonth[12]={ // array of days in month
	31,28,31,30,31,30,
	31,31,30,31,30,31};

/************************************************************************************/
/* CODE SEGMENT */
/************************************************************************************/

void setup(void) {
	Serial.begin(19200);
    Serial.println("Stuart's Orrery Controller V0.1");
    Serial.println("-------------------------------");
  
	tft.reset();
  
	uint16_t identifier = tft.readID();

	Serial.print(F("LCD driver chip: "));
	Serial.println(identifier, HEX);
    
	tft.begin(identifier);
 
	screenRotation=1;  // set initial rotation
	tft.setRotation(screenRotation); 
	tft.fillScreen(BLACK);
	screenHeight=tft.height();
	screenWidth=tft.width();
	currentcolor = RED;
	// wdt_disable(); // disable the watchdog

  // Set the processor time
	setTime(01,02,03,16,10,1964);
	pinMode(13, OUTPUT);
 
	buttonCount=6;
	// define button sizes
	if(screenRotation==0){
		buttonSize.y=screenWidth/buttonCount;
		buttonSize.x=screenHeight/buttonCount;
	}else{
		buttonSize.x=screenWidth/buttonCount;
		buttonSize.y=screenHeight/buttonCount;
	}
	showMenu();  // draw the buttons
}

void loop(){
	// row 0 = buttons
	// row 1 = clock
	// row 2 = edit up buttons
	// row 3 = list item
	// row 4 = edit down buttons
	// row 5 = buttons
	doTouch();
    gettime();   // read the time from the RTC or GPS 
    showClock(1);  // show clock on row 1    
}

/**********************************************************/
/*   Functions   */
/*   showtime(n)   Binds the clock to this location*/
/*	StuTime loadTime(n)  */
/*  saveTime(n,StuTime) saves time to mydates[n]  */


// using my own time structures as i need billion year time 
stuTime toStuTime(time_t t){
	stuTime thistime;
	thistime.millenia=0; // millenea offset
	thistime.year=year(t);
	thistime.month=month(t);
	thistime.day=day(t);
	thistime.hour=hour(t);
	thistime.minute=minute(t);
	if(DEBUGTIME){
	  Serial.println(" Convert");
  	Serial.print(thistime.year);
		Serial.println(year(t));
	Serial.print(thistime.month);Serial.println(month(t));
	Serial.print(thistime.day);Serial.println(day(t));
	Serial.print(thistime.hour);Serial.println(hour(t));
	Serial.print(thistime.minute);Serial.println(minute(t));
	}
	return thistime;
}

void showMenu(){
	// draws menu on screen
	// row 0 = buttons
	// row 1 = clock
	// row 2 = edit up buttons
	// row 3 = list item
	// row 4 = edit down buttons
	// row 5 = buttons

	int16_t colour=0;
	int bx=buttonSize.x;
	int by=	buttonSize.y;
	for(int i=0;i<6;i++){ 
	  delay(200); 
	  colour=(B11111110<<(i*2));
	  // top boxes
	  tft.fillRect(i*bx+1,0, bx-1, by, colour);
	  // bottom boxes
	  tft.fillRect(i*bx + 1, screenHeight-by, bx - 1, by, colour);

	}
	tft.setCursor(07, 05);
	tft.setTextColor(BLACK);
	tft.setTextSize(2);
	tft.print("NOW  SET DEL EDIT NEW LIST");
  
}

void showList(int n) {
	// show list of dates starting at n
	int b = buttonCount - 1;
	if (n + b > LISTSIZE) n = LISTSIZE - b; // make sure we don't overrun
	for (int i = 0; i < b; i++) {
		tft.setCursor(0, (i+1) * buttonSize.y);
		tft.setTextColor(GREEN);
		tft.setTextSize(2);
		tft.print(datestring(n+i));
	}

}

void doTouch(){
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  TSPoint tp =p; // temporary point 
  digitalWrite(13, LOW);

  // if sharing pins, you'll need to fix the directions of the touchscreen pins
  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!

	if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
		if(DEBUGTOUCH){
			Serial.print("Initial X = "); Serial.print(p.x);
			Serial.print("\tY = "); Serial.print(p.y);
			Serial.print("\tPressure = "); Serial.println(p.z);
		}
		
	  // recalibrate
    if(p.x>TS_MAXX)TS_MAXX=p.x;
    if(p.y>TS_MAXY)TS_MAXY=p.y;
    if(p.x<TS_MINX)TS_MINX=p.x;
    if(p.y<TS_MINY)TS_MINY=p.y;
  
		// handle screen rotation
		
		if(screenRotation==1){
		// tp still contains original point
		// swap x and y then invert y
	    int t=p.x;
			p.x=p.y; p.y = t;
			p.x = (map(p.x, TS_MINX, TS_MAXX, 0, screenWidth));
			p.y = (map(p.y, TS_MINY, TS_MAXY,  screenHeight,0));
		
		}	  
		else{
			// scale from 0->1023 to tft.width
			p.x = (map(p.x, TS_MINX, TS_MAXX, 0, screenWidth));
			p.y = (map(p.y, TS_MINY, TS_MAXY, 0, screenHeight));
		}
		p.x+=xoffset;
		p.y+=yoffset;
   
		touched=p; // save the position
		if(DEBUGTOUCH){
			Serial.print(" Width:"); Serial.print(screenWidth);
			Serial.println(" Height"); Serial.print(screenHeight);
    
			Serial.print(" Final("); Serial.print(p.x);
			Serial.print(", "); Serial.print(p.y);
			Serial.println(")");
		
			Serial.print(" Max("); Serial.print(TS_MAXX);
			Serial.print(", "); Serial.print(TS_MAXY);
			Serial.print(")");
    
			Serial.print(" Min("); Serial.print(TS_MINX);
			Serial.print(", "); Serial.print(TS_MINY);
			Serial.println(")");
			Serial.print(" Button("); Serial.print(buttonSize.x);
			Serial.print(", "); Serial.print(buttonSize.y);
			Serial.println(")");
    
			Serial.print(" Menu:"); Serial.println(menuState);
    
		}
		   // check button events
		int grid = p.x/buttonSize.x;

		grid+=(p.y/buttonSize.y*10);
		buttonEvent(grid);
		// draw dot where touched	
		if (((p.y-PENRADIUS) > 0) && ((p.y+PENRADIUS) < tft.height())) {
		  tft.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
		  
		}
	}

  
}

 boolean isbst(time_t t){ 
 // checks if date is in bst
        if (month(t) < 3 || month(t) > 10)  return false; 
        if (month(t) > 3 && month(t) < 10)  return true; 
        int previousSunday = day(t) - weekday(t);
        if (month(t) == 3) return previousSunday >= 25;
        if (month(t) == 10) return previousSunday < 25;
        return false; // this line is never gonna happen
    }


void gettime(){ 
	//      Load the time variables from System clock   //

	millisNow=millis(); // timer for local loop triggers
	timeNow=now(); // get the current time stamp

	// write clock to myDates[clockPtr];
	myDates[clockPtr]= toStuTime(timeNow);
	// adjust for bst
	if(isbst(timeNow)){
	 myDates[clockPtr].hour++;  
      if(DEBUGTIME)Serial.println("Date is in BST");
    }else{
      if(DEBUGTIME)Serial.println("Date is not in BST");
    };
}


void print2digits(int number) {
  if (number > 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}


void checkReport() {
    if( millisNow> reportTime){ // if we're due
      reportTime=millisNow+reportInterval; // reset interval
      Serial.println("*******************************");
      
      Serial.print("Onboard Clock:"+String(hour(timeNow))+":"+ String(minute(timeNow))) +":"+ String(second(timeNow));  // Print system time
      if(isAM(timeNow)) Serial.println("AM");
      if(isPM(timeNow)) Serial.println("PM");
      // now() time in seconds since 1/1/70
      Serial.println(String(day(timeNow))+":"+String(month(timeNow))+":"+String(year(timeNow)));             // The day now (1-31)
      Serial.println("DOTW:"+String(weekday(timeNow)));         // Day of the week, Sunday is day 1
      //Serial.println(String(dayStr(weekday(time_now)))+" "+String(day(time_now))+String(monthStr(month(time_now))));
      //Serial.println(String( monthShortStr(month(time_now)))+" "+String(dayShortStr(weekday(time_now))));
      Serial.println("Time Status:"+String(timeStatus()));
      Serial.println("Time Now:"+String(timeNow));
      Serial.print(tmYearToCalendar(tm.Year));
      Serial.println();
    }
}


String timestring(int n)   {
	String s= 
	twochars(myDates[n].hour)
		+":"+twochars(myDates[n].minute)
		+":"+twochars(myDates[n].second);
	return s;
}

String longDateString(int n){
  String thisstring;
    // thisstring=dayShortStr(weekday(t));
    //  thisstring+=" ";
      thisstring+=String(myDates[n].day);
      thisstring+=" ";
      thisstring+=String(monthShortStr(myDates[n].month));
      thisstring+=" ";
      thisstring+=String(myDates[n].year);
  return thisstring;
}

String datestring(int n){   // return short date string for myDates[n]
  String thisstring;
    thisstring=
	thisstring+=myDates[n].year;
      thisstring+=":";
      thisstring+=twochars(myDates[n].month);
      thisstring+=":";
      thisstring+=twochars(myDates[n].day);
      
  return thisstring;
}

String twochars(int number){
  if(number==0) {
    return"00";
  }
  else {
    if (number > 0 && number < 10)  {
      return "0"+String(number);
    }
    else  {
      return String(number);
    }
  }
}

void adjHours(int n, int a) { // adjust hours n by a
	a = a % 24; // modulo 24
	myDates[n].hour += a;  // adjust
	if (myDates[n].hour <0)
	{
		myDates[n].hour+=24;
	}
	if (myDates[n].hour > 24) myDates[n].hour -= 24;

	setClock(n);
}

void adjMinutes(int n, int a) { // adjust hours n by a
	a = a % 60; // modulo 24
	myDates[n].minute += a;  // adjust
	if (myDates[n].minute <0)
	{
		myDates[n].minute += 60;
	}
	if (myDates[n].minute > 60) myDates[n].minute -= 60;

	setClock(n);
}
void adjDays(int n, int a) { // adjust days n by a
	int t = daysInMonth[myDates[n].month];

	a = a % t; // modulo 24
	myDates[n].day += a;  // adjust
	if (myDates[n].day <0)
	{
		myDates[n].day += t;
	}
	if (myDates[n].day > t) myDates[n].day -= t;

	setClock(n);
}

void adjMonths(int n, int a) { // adjust months n by a
	a = a % 12; // modulo 24
	myDates[n].month += a;  // adjust
	if (myDates[n].month <0)
	{
		myDates[n].month += 12;
	}
	if (myDates[n].month > 12) myDates[n].month -= 12;

	setClock(n);
}

void adjYears(int n, int a) { // adjust hours n by a
	a = a % 1000; // modulo 24
	myDates[n].year += a;  // adjust
	if (myDates[n].year <0)
	{
		myDates[n].year += 1000;
	}
	if (myDates[n].year > 1000) myDates[n].year -= 1000;

	setClock(n);
}

void adjMillenia(int n, int a) { // adjust hours n by a
	
	myDates[n].millenia += a;  // adjust
	
	setClock(n);
}



void setClock(int n){  // set system clock from myDate
	setTime(myDates[n].hour, myDates[n].minute, myDates[n].second, myDates[n].day, myDates[n].month, myDates[n].year);
}

void clearScreen(){
	tft.fillRect(0, (buttonSize.y)+2, screenWidth, screenHeight-buttonSize.y, BLACK);
	 
}

void buttonEvent(int inp){
  //dm.setLED( 1,inp);
	if(DEBUGBUTTONS){
	  Serial.print("Button:");
	  Serial.println(inp);
	}
	if(inp==0)menuState=0;  // NOW button resets state
	switch (menuState) {
		case 0: {// Root Display 
			switch (inp) {
				case 0: // Now
				{
					menuState=0;
					clearScreen();
				}
				break;
		
				case 1: // Set
				{
					menuState=1;
					clearScreen();
					drawEditButtons(2); // draw the up/down triangles starting at row 2
				
				}
				break;
				case 2: // Delete
				{
					menuState=2;
				}
				break;
				case 3: // Edit
				{
					menuState=3;
				}
				break;
				case 4: // New
				{
					menuState=4;
				}
				break;
				case 5: // List
				{
					menuState=5;
					showList(listPtr);
				}
				break;
				case 6: //list
				{
					
				}
				break;
				
			}
		}
		break;
		case 1:{ // clock set
			switch (inp) {
				case 40: { // dec year
					adjYears(clockPtr, -1);
					Serial.println("Dec year!")	;
				}
				break;
				
				case 41:  { // decrement months
					adjMonths(clockPtr, -1);
				}
				break;
				
				case 42: { // dec days
					adjDays(clockPtr, -1);
				}
				break;
				case 43:  	  { // decrement Hours
					adjHours(clockPtr, -1);
				}
				break;
				case 44:  	  { // decrement Minutes
					adjMinutes(clockPtr, -1);
				}
				break;
				case 20: { // inc year
					adjYears(clockPtr, 1);
					Serial.println("Inc year!");
				}
						 break;

				case 21: { // inc months
					adjMonths(clockPtr, 1);
				}
						 break;

				case 22: { // inc days
					adjDays(clockPtr, 1);
				}
						 break;
				case 23: { // inc Hours
					adjHours(clockPtr, 1);
				}
						 break;
				case 24: { // inc Minutes
					adjMinutes(clockPtr, 1);
				}
						 break;
			}
		}
		break;
	}
  
}

void drawEditButtons(int row){  // row started at 2
  // show edit triangles
    for(int i=0; i<buttonCount; i++) {
		// down triangles
      tft.drawTriangle(
        (i*buttonSize.x)+(buttonSize.x/2)    ,(row+3)*buttonSize.y , // peak
        i*buttonSize.x, (row+2)*buttonSize.y+1, // bottom left
        (i+1)*buttonSize.x, (row+2)*buttonSize.y+1, // bottom right
        // tft.color565(0, 0, i*24)
        RED
    );
	  // up triangles
	 tft.drawTriangle(
        (i*buttonSize.x)+(buttonSize.x/2)    , row*buttonSize.y , // peak
        i*buttonSize.x, (row+1)*buttonSize.y, // bottom left
        (i+1)*buttonSize.x, (row+1)*buttonSize.y, // bottom right
        // tft.color565(0, 0, i*24)
        RED
    );
    Serial.println("edit buttons drawn");
    }
}

void clearRow(int r) {   // Clear row 
	tft.fillRect(0, (r * buttonSize.y), tft.width(), buttonSize.y, BLACK);

}


void showClock(int row) {
  // for serial debugging
  if(DEBUGTIME){
   Serial.print(timestring(clockPtr));
   }
	if(myDates[clockPtr].second!=lastsecond){
		clearRow(row);
		// tft.fillRect(0, (3*buttonSize.y)+2, tft.width(),buttonSize.y-1, BLACK);
		tft.setCursor(0,(3*buttonSize.y)+4);
		tft.setTextColor(GREEN);
		tft.setTextSize(2);
		tft.print(datestring(clockPtr));
		tft.setTextSize(3);	  
		tft.print(timestring(clockPtr));
		if(DEBUGCLOCK){
			Serial.print("State:"+String(menuState)+" timestring:"+timestring(clockPtr));
		}
	  lastsecond=myDates[clockPtr].second;
	}
}

