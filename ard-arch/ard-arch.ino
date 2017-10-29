#define FASTLED_ALLOW_INTERRUPTS 0  // Fully disable interrupts while writing to neopixels
#include "FastLED.h"
#include <avr/wdt.h>

FASTLED_USING_NAMESPACE
#define arduino_address 3
#define NUM_LEDS    210   //0.3W per LED, so, 
#define DATA_PIN    6
#define enable_rs485pin 2
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS  200
#define SLICES_PER_SECOND 20 // The time for each function to run is given as a quantity of these (20 pieces = one second of run time)
#define BAUD_RATE 57600
#define SERIAL_TIMEOUT 250


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
const do_reboot_t do_reboot = (do_reboot_t)((FLASHEND-511)>>1);  // Aim for the location of the bootloader (Flashend-size of bootloader in bytes)

bool gReverseDirection = false;


// Globals
int arch_location[] = {0,NUM_LEDS/3, 2*NUM_LEDS/3};
CRGB leds[NUM_LEDS];
byte command[6];
unsigned long timesnap;



void setup() {

  wdt_disable();
//  interrupts();

  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness( BRIGHTNESS );

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
    leds[0]=CRGB::Red;
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
        // 0x03 0x00 0x00 0x00 0x00 0x00
        if (command[2] == 0x05)  
        {
          // Show me what's up:
          leds[NUM_LEDS/3]=CRGB(255,0,0);
          FastLED.show();
          
          jump_to_bootloader();
          break;
        }

        // Blank all lights
        // 0x03 0x00 0x01 0x00 0x00 0x00  : single
        // 0x00 0x00 0x01 0x00 0x00 0x00  : Universal
        if (command[2] == 0x01)  // Blank
        {
          fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
          FastLED.show();
          break;
        }

        // Set Brightness
        // 0x03 0x00 0x02 0xff 0x00 0x00
        if (command[2] == 0x02)
        {
          FastLED.setBrightness( command[3] );
          FastLED.show();
          break;
        }
        

      // 0x03 0x01 0x00 0x00 0xff 0x00
      case 0x01:  // Fill all leds with solid color
        fill_solid(leds,NUM_LEDS,CRGB(command[2],command[3],command[4]));
        FastLED.show();
        break;

      // Fill arch# command[2] with solid color CRGB(command[3],command[4],command[5]
      // 0x03 0x02 0x01 0x00 0x00 0xff
      case 0x02:
        fill_solid(&leds[arch_location[command[2]]],NUM_LEDS/3,CRGB(command[3],command[4],command[5]));
        FastLED.show();
        break;

      // Static Rainbow starting with hue command[2]
      // 0x03 0x03 0xcc 0x00 0x00 0x00
      case 0x03:
        fill_rainbow( leds, NUM_LEDS, command[2], 15);
        FastLED.show();
        break;

      // 0x03 0x04 0x30 0x00 0x05 0x05
      case 0x04:  // Moving Rainbow
/*
 *    command[2] = number of 1/20 seconds to run
 *    commnad[3] = starting hue
 *    command[4] = speed of rainbow change (ms between updates) 
 *    command[5] = hue difference between updates
 */
        timesnap = millis();  // Time we start

        while ( (millis() - timesnap)  < (command[2] * 1000 / SLICES_PER_SECOND) )  // while we still have time
        {    
          fill_rainbow( leds, NUM_LEDS, command[3], 15);
          FastLED.show();
          command[3]+=command[5];
          FastLED.delay(command[4]);
        }
        break;
        


      // Rainbow with sparkles
      // 0x03 0x05 0x24 0x00 0x03 0x03
      case 0x05:
/*
 *    command[2] = number of 1/10 seconds to run
 *    commnad[3] = starting hue
 *    command[4] = speed of rainbow change (ms between updates) 
 *    command[5] = hue difference between updates
 */
        timesnap = millis();  // Time we start
        
        while ( (millis() - timesnap)  < (command[2] * 1000 / SLICES_PER_SECOND) )  // while we still have time
        {    
          fill_rainbow( leds, NUM_LEDS, command[3], 15);

          if( random8() < 30) {
            leds[ random16(NUM_LEDS) ] += CRGB::White;
          }
          
          FastLED.show();
          command[3]+=command[5];
          FastLED.delay(command[4]);
        }
        break;


      // Cylon for command[2] 20ths of a second from command[3] to command[4] with hue command[5]
      // 0x03 0x06 0x14 0x00 0xd2 0xcc
      // command[2] is how many time slices to run where a second has SLICES_PER_SECOND many slices.
      // There are NUM_LEDS pixels where each pixel takes 30uS to update
      // It may be that the cylon effect can't be done fast enough if the cylon pulse goes one at a time.
      // For example, for 210 pixels (arches), the total time would be 210 * 0.00003 = 6.3mS per frame
      // 0.0063 * 210 iterations for a cylon sweep = 1.323 seconds to do a cylon sweep with no added delay not including any microprocessor work in between.
      // For this reason we may need to skip pixels some times without having any added delay or, if we have plenty of time, don't skip and add delays.

      case 0x06:
      {
        
        // Clens inputs
        if (command[3] >= NUM_LEDS)
          command[3]=NUM_LEDS-1;
        if (command[4] >= NUM_LEDS)
          command[4]=NUM_LEDS-1;
        
        // Blank
        fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
        FastLED.show();

        
        // Calculate the amount of time required and see which path we need to go down (added delay or skipping without delay)

        // Is the time requested for this cylon to run greater than the fastest no-delay time?

        //     <-------Requested time in uS--------->     <---Time to Travel Distance w/o delay (uS)--->
        //     Num slices * <-----uS per slice------>     <---- pixels to travel ---> * <-uS per frame->
        //     command[2] * 1000000/SLICES_PER_SECOND     (abs(command[4]-command[3]) * NUM_LEDS * 30 )

        if   ( ((unsigned long)command[2] * 1000000 / (unsigned long)SLICES_PER_SECOND) < (abs(command[4]-command[3]) * (unsigned long)NUM_LEDS * 30) )  // Do we have to skip some pixels?
        {
          
            // If we're here then we need to skip some pixels.
            // The idea is we want to use sequences of integers to approximate decimals.
            // For example, if we need to move forward 1.5 pixels on average, we would move forward by 1,2,1,2,1,2,etc...
            // So, we first need to find out the average (decimal) rate of movement [mS/Pixel]
            // We do this by finding the ratio between the two values compared above, essentially, how many times over the theoretical maximum am I?
            // (abs(command[4]-command[3]) * NUM_LEDS * 30 ) / (command[2] * 1000000 / SLICES_PER_SECOND)
            float avg_pixel_per_cycle = ((float)abs(command[4]-command[3]) * NUM_LEDS * 30 ) / ((float)command[2] * 1000000 / SLICES_PER_SECOND);

            // Now what we can do is keep a running total of our accumulated error.  Once it goe
            float accumulated_error=0;

            // But, if we have enough cumulative error from the past including the 0.76 from this next jump that goes over 1, well, go 4 pixels forward instead and
            // remove the appropriate amount of cumulative error
            int current_position=command[3]; // Set the starting point
            int previous_position = current_position; // Keep track of the last point so we can draw an LED segment from previous_position to current_position
            while (current_position != command[4])
            {

                if (command[3] < command[4]) // going forward?
                  current_position+=(int)avg_pixel_per_cycle; // Move the integer component of avg_pixel_per_cycle
                else
                  current_position-=(int)avg_pixel_per_cycle;// Move the integer component of avg_pixel_per_cycle

                // Save error
                accumulated_error+=avg_pixel_per_cycle - (int)avg_pixel_per_cycle; 

                // Don't let the accumulated error get more than 0.5 away from 0
                // This allows for getting a little ahead of the target with the overall goal of
                // staying as close as possible to it.
                while (accumulated_error > 0.5)
                {
                  if (command[3] < command[4]) // going forward?
                    current_position+=1;
                  else
                     current_position-=1;
                     
                  accumulated_error-=1;
                }

                // If this is the last step, we might have overshot, let's bring it back so we can exit this loop
                if (command[3] < command[4]) // going forward?
                {
                  if (current_position > command[4]) // If we overshot
                    current_position=command[4];
                }
                else
                {
                   if (current_position < command[4])
                    current_position=command[4];
                }
                

                // Modify the lights accordingly
                fadeToBlackBy( leds, NUM_LEDS, 20); // Dim what's been before by 20/255 amount
                
                if (command[3] < command[4]) // going forward?
                  fill_solid(&leds[previous_position], current_position-previous_position+1, CHSV( command[5], 255, 255));
                else
                  fill_solid(&leds[current_position],  previous_position-current_position+1, CHSV( command[5], 255, 255));
                  
                FastLED.show();

                previous_position = current_position; // Save for next loop.
            } // Cycle loop
        }  // If we're skipping pixels because of speed limitations
        else  // Going slow enough that we need to add a delay...
        {

            int current_position=command[3]; // Set the starting point
            unsigned long delay_value = (command[2]*1000/SLICES_PER_SECOND)/abs(command[4]-command[3]);

            while (current_position != command[4])
            {
              timesnap = millis();  // Time we start

              if (command[3] < command[4]) // going forward?
                current_position+=1;
              else
                current_position-=1;
                
              fadeToBlackBy( leds, NUM_LEDS, 20); // Dim what's been before by 20/255 amount
              leds[current_position] = CHSV(command[5],255,255);
              FastLED.show();

              while ( (millis() - timesnap) < delay_value );  // Wait here until it's time again to start
            }
        } // delay added

        // Blank it.
        fill_solid(leds,NUM_LEDS,CRGB(0,0,0));
        FastLED.show();
      } // case cylon
      break;

 



      // Fire
      // command[2] = Time in 1/10 sec for completion
      // command[3] = ms delay between frames
      // command[4] = Cooling (55)  Less = taller flames
      // command[5] = Sparking (120)  More = roaring fire, less = flickery fire
      // 0x03 0x07 0xff 0x03 0x40 0x60
      case 0x07:
   
        fill_solid(leds,NUM_LEDS,CRGB(150,150,150));  // Get rid of top of arch pixels if they are on...
        FastLED.show(); // display this frame
      
      timesnap = millis();  // Time we start
      while ( (millis() - timesnap)  < (command[2] * 1000 / SLICES_PER_SECOND) )  // while we still have time
      {
    
        static byte heat[NUM_LEDS/6];
        // Step 1.  Cool down every cell a little
        for( int i = 0; i < (NUM_LEDS/6); i++) {
          heat[i] = qsub8( heat[i],  random8(0, ((command[4] * 10) / (NUM_LEDS/6)) + 2));
        }
      
        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for( int k= (NUM_LEDS/6) - 1; k >= 2; k--) {
          heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
        }
        
        // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        if( random8() < command[5] ) {
          int y = random8(7);
          heat[y] = qadd8( heat[y], random8(160,255) );
        }
    
        // Step 4.  Map from heat cells to LED colors
        for( int j = 0; j < (NUM_LEDS/6); j++) {
          CRGB color = HeatColor( heat[j]);
          int pixelnumber;
          if( gReverseDirection ) {
            pixelnumber = ((NUM_LEDS/6)-1) - j;
          } else {
            pixelnumber = j;
          }
          leds[pixelnumber] = color;
        }

        // Mirror half arch to other half
        for (int i=0;(NUM_LEDS/3 - i)>2;i++)
          leds[NUM_LEDS/3-i] = leds[i];

        // Copy first arch to other arches  0-70 = 71-140 = 141-210
        for (int i=0;(NUM_LEDS/3 - i)>2;i++)
        {
          leds[(1*(NUM_LEDS/3))+i+1] = leds[i];  // 1*70+0+1 = 71
          leds[(2*(NUM_LEDS/3))+i+1] = leds[i];
        }

        
        FastLED.show(); // display this frame
        FastLED.delay(command[3]);
        
      }  // while we still have time
      break;


      // Fill each arch with its own color
      // 0x03 0x08 0xff 0xff 0xff 0x00
      case 0x08:
        // Fill all with the last color
        fill_solid(leds,NUM_LEDS,CHSV(command[4],255,255));
        // Fill 2/3 with the second color
        fill_solid(leds,2*NUM_LEDS/3,CHSV(command[3],255,255));
        // Fill 1/3 with the first color
        fill_solid(leds,NUM_LEDS/3,CHSV(command[2],255,255));

        FastLED.show();
       break;


       // Incoming!!
       // 0x03 0x09 0x14 0x00 0x00 0xff
       // 2 = Time slices
       // 3 = Hue of bomb
       // 4 = Saturation of bomb
       // 5 = Value of bomb
       case 0x09:
       {
          unsigned long delay_value = command[2]*1000/SLICES_PER_SECOND/(NUM_LEDS/6);
        
          // Going to start at apex and work my way down
          // Apex = NUM_LEDS/6;
          for (int i=(NUM_LEDS/6);i>=0;i--)
          {
              // Due the first half of the first arch
              fadeToBlackBy(leds, NUM_LEDS, 100);  // Dim what's been done before
              leds[i] += CHSV(command[3],command[4],command[5]);  // Activate latest position

              // Mirror first have of the first arch to other side of first arch
              for (int i=0;(NUM_LEDS/3 - i)>2;i++)
                  leds[NUM_LEDS/3-i] = leds[i];

              // Copy first arch to other arches  0-70 = 71-140 = 141-210
              for (int i=0;(NUM_LEDS/3 - i)>2;i++)
              {
                leds[(1*(NUM_LEDS/3))+i+1] = leds[i];  // 1*70+0+1 = 71
                leds[(2*(NUM_LEDS/3))+i+1] = leds[i];
              }
             
              FastLED.show();
              delay(delay_value);
           }
       }

        break;





       
        
    }   //switch

    command[1]=0xff;  // Make it so command doesn't run again
  }

}


void jump_to_bootloader()
{
  // Bootloader assumes a few things we need to setup before jumping.
  noInterrupts();
  MCUSR=0;
  do_reboot();  // jump to bootloader

}

 
