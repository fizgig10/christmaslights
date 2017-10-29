#define FASTLED_ALLOW_INTERRUPTS 0  // Fully disable interrupts while writing to neopixels
#include "FastLED.h"
#include <avr/wdt.h>
FASTLED_USING_NAMESPACE
#define arduino_address 4
#define leds_per_cane 24
#define num_canes 6
#define NUM_LEDS    leds_per_cane*num_canes   //Each can has 24 LEDs 
#define DATA_PIN    6
#define enable_rs485pin 2
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS         255
#define SLICES_PER_SECOND 20 // The time for each function to run is given as a quantity of these (20 pieces = one second of run time)
#define BAUD_RATE 57600
#define SERIAL_TIMEOUT 250

#define blank_all fill_solid(leds,NUM_LEDS,CRGB(0,0,0)); FastLED.show()
#define flush_serial while(Serial.available()){Serial.read();}

/*
 *  WS2812B Notes
 * Time: Neopixel (WS2812) takes 30us per pixel
 * -> 210 pixels = 6300us = 6.3ms = 158.7 Hz max refresh rate
 * 
 * Power: Neopixel can draw 60mA at max white
 * -> 210 pixels = 12600mA = 12.6amp = 63W max draw
 */

/* This section is to make a place to jump to
 *  if we need to run the bootloader to update
 */
typedef void (*do_reboot_t)(void);
const do_reboot_t do_reboot = (do_reboot_t)((FLASHEND-511)>>1);

bool gReverseDirection = false;


// Globals
CRGB leds[NUM_LEDS];
byte command[6];
unsigned long timesnap;



void setup() {
  wdt_disable();
//  interrupts();

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness( BRIGHTNESS );

  pinMode(13, OUTPUT);  // Take control of onboard LED
  
  // Blink onboard LED and 1st Neopixel to show where we are
  fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
  for (uint8_t i=0;i<3;i++)
  {
    digitalWrite(13, HIGH);
    leds[0]=CRGB::White;
    FastLED.show();
    delay(1000);              

    digitalWrite(13, LOW);  
    leds[0]=CRGB::Black;
    FastLED.show();
    delay(200); 
  }
  
  // Enable RS485 chip
  pinMode(enable_rs485pin, OUTPUT);
  digitalWrite(enable_rs485pin, LOW);  // receive = low

  Serial.begin(BAUD_RATE);
  Serial.setTimeout(SERIAL_TIMEOUT);
  
}



void control_cane_x(int cane_num, int turn_on)
{
  if (turn_on==1)
    fill_solid(&leds[cane_num*leds_per_cane],leds_per_cane,CRGB(255,0,0)); 
  else
    fill_solid(&leds[cane_num*leds_per_cane],leds_per_cane,CRGB(0,0,0)); 
}



void copy_cane_0_to_all()
{

    // if leds_per_cane = 4
    // source_cane_number = 1  => 4  (skipping 0-3 belonging to cane 0)
    // destination_cane_number = 2  => 8 (skipping 0-3 and 4-7 belonging to canes 0 and 1)

    // source: leds_per_cane*source_cane_number
    // destin: leds_per_cane*source_cane_number
    
    // Copy 1st cane to 2nd
    memmove( &leds[leds_per_cane], &leds[0], (leds_per_cane) * sizeof( CRGB) );
    // Copy 1st cane to 3rd
    memmove( &leds[leds_per_cane*2], &leds[0], (leds_per_cane) * sizeof( CRGB) );
    // Copy 1-2-3 to 4-5-6
    memmove( &leds[leds_per_cane*3], &leds[0], (leds_per_cane*3) * sizeof( CRGB) );
}




void crawl(unsigned int slices_to_run_for, int ms_per_cycle)
{
    timesnap = millis();  // Time we start

    
    // If ( (ms transpired so far) < (total time this function should run) )
    while ( (millis() - timesnap)  < (slices_to_run_for * 1000 / SLICES_PER_SECOND) )  // while we still have time
    {  
      fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
      for (int i=0;i<=(leds_per_cane-1);i++)
      {
        leds[i]=CRGB::Red;
        i=i+1;
      }
      copy_cane_0_to_all();   
      FastLED.show();
    
      delay(ms_per_cycle);
      
      fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
      for (int i=1;i<=(leds_per_cane-1);i++)
      {
        leds[i]=CRGB::Red;
        i=i+1;
      }
      copy_cane_0_to_all();
      FastLED.show();
      delay(ms_per_cycle);
    }
}



void stack(unsigned int slices_to_run_for)
{
    unsigned long ms_per_frame = 0;
    
    // First decide how long to wait between frames
    // Total number of iterations = NUM_LEDS + NUM_LEDS-1 + NUM_LEDS-2....
    int total_frames_per_stack=0;
    for (int i=0;i<=leds_per_cane;i++)
      total_frames_per_stack+=i;
    // total_frames_per_stack now contains the number of frames this animation will take.
    // To fine the delay between frames...
    ms_per_frame=slices_to_run_for*(1000/SLICES_PER_SECOND) / total_frames_per_stack;


    for (int current_stack_target = 0; current_stack_target <= leds_per_cane-1; current_stack_target++)
    {

      for (int i=leds_per_cane-1;i>=current_stack_target;i--)
      {
        fill_solid(leds,NUM_LEDS,CRGB(0,0,0));  // Blank everything
        fill_solid( &(leds[0]), current_stack_target, CRGB::Red );  // Fill up from the bottom the number of leds we've already stacked
        leds[i]=CRGB::Red;  // Now fill in the moving dot
        copy_cane_0_to_all();

        FastLED.show();
        delay(ms_per_frame);
      }
    
    }
  
}




/*
   *    command[2] = slices_to_run_for
   *    command[3] = Start led #
   *    command[4] = End led #
   */
void cylon(byte slices_to_run_for, byte start_LED_num, byte end_LED_num)
{
  unsigned long delay_value = abs(slices_to_run_for*1000/SLICES_PER_SECOND/(end_LED_num-start_LED_num+1));
  int position = start_LED_num;
  bool keep_going=true;

  fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
  FastLED.show();
   
  while (keep_going)
  {

    if (position == end_LED_num)
      keep_going = false;
    
    timesnap = millis();  // Time we start
    fadeToBlackBy(leds, NUM_LEDS, 100);  // Dim what's been done before
    leds[position] += CRGB::Red;
    copy_cane_0_to_all();
    FastLED.show();

    if (end_LED_num > start_LED_num)
      position++;
    else
      position--;
    // Need to waith command[2]*100/travel ms before continuing... where travel = command[4]-command[3]+1
    while ( (millis() - timesnap) < delay_value );  // Wait here until it's time again to start
  }
}



void swipe(byte slices_to_run_for, byte dir)   //direction: 1=up,2=down,3=right,4=left
{
  unsigned long delay_value;
  int num_steps;
  
  
  num_steps=24; // catchall to be changed below
  
  switch (dir)
  {
      case (1):
        num_steps = 19;
        break;
      case (2): // up or down
        num_steps = 19;
        break;
      case (3):
        num_steps = 8*num_canes; 
        break;
      case (4): // left or right
        num_steps = 8*num_canes; 
        break;
  }

  delay_value = slices_to_run_for*1000/SLICES_PER_SECOND/num_steps/2;

  if (dir==1)
  {
    // Assuming candycanes point to the left
    // Just have to worry about one cane then copy to others
    // Order = 1,2,3,...,16 & 23, 17 & 22, 18 & 21, 19 & 20 
    for (int i=0;i<=num_steps;i++)
    {
      if (i < 16)
        leds[i]=CRGB::Red;
      else if (i >= 16)
      {
        leds[i]=CRGB::Red; 
        leds[leds_per_cane-1-(i-16)]=CRGB::Red; 
      }
      copy_cane_0_to_all();
     FastLED.delay(delay_value);
    }
    for (int i=0;i<=num_steps;i++)
    {
      if (i < 16)
        leds[i]=CRGB::Black;
      else if (i >= 16)
      {
        leds[i]=CRGB::Black; 
        leds[leds_per_cane-1-(i-16)]=CRGB::Black; 
      }
      copy_cane_0_to_all();
     FastLED.delay(delay_value);
    }
  }

  if (dir==2)
  {
    // Assuming candycanes point to the left
    // Just have to worry about one cane then copy to others
    // Order = 1,2,3,...,16 & 23, 17 & 22, 18 & 21, 19 & 20 
    for (int i=num_steps;i>=0;i--)
    {
      if (i < 16)
        leds[i]=CRGB::Red;
      else if (i >= 16)
      {
        leds[i]=CRGB::Red; 
        leds[leds_per_cane-1-(i-16)]=CRGB::Red; 
      }
      copy_cane_0_to_all();
     FastLED.delay(delay_value);
    }
    for (int i=num_steps;i>=0;i--)
    {
      if (i < 16)
        leds[i]=CRGB::Black;
      else if (i >= 16)
      {
        leds[i]=CRGB::Black; 
        leds[leds_per_cane-1-(i-16)]=CRGB::Black; 
      }
      copy_cane_0_to_all();
     FastLED.delay(delay_value);
    }
  }
   
}


void demo()
{
  crawl(0x30,0x64);
  crawl(0x30,0x34);
  
  stack(0x32);
  stack(0x10);
  
  cylon(0x0a,0x00,leds_per_cane);
  cylon(0x0a,leds_per_cane,0x00);
  
  cylon(0x08,0x00,leds_per_cane);
  cylon(0x08,leds_per_cane,0x00);
  
  cylon(0x06,0x00,leds_per_cane);
  cylon(0x06,leds_per_cane,0x00);

  cylon(0x04,0x00,leds_per_cane);
  cylon(0x04,leds_per_cane,0x00);
  
  cylon(0x02,0x00,leds_per_cane);
  cylon(0x02,leds_per_cane,0x00);

  cane_march(05,0,5);
  cane_march(05,0,5);
  cane_march(05,0,5);

  swipe(20,1);
  swipe(20,2);
  swipe(20,1);
  swipe(20,2);

  fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
  FastLED.show();
  
}


void cane_march(byte slices_to_run_for,byte start_cane,byte end_cane)
{
  unsigned long delay_value = slices_to_run_for*1000/SLICES_PER_SECOND/num_canes;
  
  for (int i=start_cane;i<=end_cane;i++)
  {
    blank_all;
    control_cane_x(i,1);
    FastLED.show();
    FastLED.delay(delay_value);
    blank_all;
  }
}



void jump_to_bootloader(){

  // Bootloader assumes a few things we need to setup before jumping.

  noInterrupts();
  MCUSR=0;
  do_reboot();  // jump
}


void loop()
{


  flush_serial;
  Serial.readBytes(command,6);

  // First check to see if someone else is getting programmed.  If so, turn off for a bit and flush the uart...
  // If this command isn't for us and it is a reset to bootloarder command
  if ( (command[0]!=arduino_address) && (command[1]==0x00) && (command[2]==0x05) )
  {
    // Show status
    fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
    leds[0]=CRGB(255,0,0);
    FastLED.show();
    digitalWrite(13, HIGH);

    delay(20000);  // Wait for all programming to finish

    while(Serial.available()){Serial.read();}  // Clear any garbage out
    digitalWrite(13, LOW);
    leds[0]=CRGB(0,0,0);
    FastLED.show();
    command[0]=0xfe; // Make this something other than 0 for when we start so we don't just go to bootloader... 
    command[1]=0xfe;
  }
  else if ( (command[0]==0x00) || (command[0]==arduino_address) )  // If it's a broadcast or just to me...
  {
    switch (command[1])
    {

      case 0x00:  // Control

        // Reboot to bootloader to program
        // 0x04 0x00 0x05 0x00 0x00 0x00
        if (command[2] == 0x05)  
        {
          // Show me what's up:
          leds[leds_per_cane]=CRGB(255,0,0);
          FastLED.show();
          
          jump_to_bootloader();
          break;
        }

        // Blank all lights
        // 0x04 0x00 0x01 0x00 0x00 0x00
        if (command[2] == 0x01)  // Blank
        {
          fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
          FastLED.show();
          break;
        }

        // Set Brightness
        // 0x04 0x00 0x02 0xff 0x00 0x00
        if (command[2] == 0x02)
        {
          FastLED.setBrightness( command[3] );
          FastLED.show();
          break;
        }

      break; // End of control case
        

      // 0x04 0x01 0x00 0x00 0xff 0x00
      case 0x01:  // Fill all leds with solid color
        fill_solid(leds,NUM_LEDS,CRGB(command[2],command[3],command[4]));
        FastLED.show();
        break;


      // 0x04 0x02 0x30 0x64 0x00 0x00
      case 0x02:  // Marquee Light Border
  /*
   *    command[2] = Number of time slices to run for
   *    command[3] = ms per blink (smaller makes it blink faster)
   */
        crawl(command[2],command[3]);
        blank_all;
        break;


      // 0x04 0x03 0x32 0x30 0x00 0x00
      case 0x03: // Stack
  /*
   *    command[2] = Number of time slices to run for
   */    
      stack(command[2]);
      blank_all;
      break;


      // 0x04 0x04 0x0a 0x00 0x17 0x00 
      case 0x04:  // Cylon entire chain
  /*
   *    command[2] = Number of time slices to run for
   *    command[3] = Start led #
   *    command[4] = End led #
   */
        cylon(command[2],command[3],command[4]);
        break;

       // 0x04 0x05 0x50 0x00 0x00 0x00
       // command[2] = Number of time slices to run for
       // command[3] = Start cane
       // command[4] = end cane
      case 0x05:
        cane_march(command[2],0,5);
        break;

      // 0x04 0x06 0x50 0x01 0x00 0x00
      case 0x06:
        swipe(command[2],command[3]);
        break;



      // Turn on Cane(s) (Random Access)
      // 0x04 0x07 0x3F 0x00 0x00 0x00
      // command[2] = bitmask of canes (cane#1 = LSB)
      case 0x07:
        if (command[2] & 0b00000001)
          control_cane_x(0,1);
        else
          control_cane_x(0,0);
          
        if (command[2] & 0b00000010)
          control_cane_x(1,1);
        else
          control_cane_x(1,0);
          
        if (command[2] & 0b00000100)
          control_cane_x(2,1);
        else
          control_cane_x(2,0);
          
        if (command[2] & 0b00001000)
          control_cane_x(3,1);
        else
          control_cane_x(3,0);
          
        if (command[2] & 0b00010000)
          control_cane_x(4,1);
        else
          control_cane_x(4,0);
          
        if (command[2] & 0b00100000)
          control_cane_x(5,1);
        else
          control_cane_x(5,0);

        FastLED.show();
      break;

        // 0x04 0xff 0x0a 0x00 0x17 0x00 
       case 0xff:  //demo
        demo();
        break;
      
        
    }  //switch

    command[1]=0xfe;  // Make it so command doesn't run again
  }

}

