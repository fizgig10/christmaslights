#include <avr/wdt.h>
#define arduino_address 2
#define SLICES_PER_SECOND 20 // The time for each function to run is given as a quantity of these (20 pieces = one second of run time)
#define BAUD_RATE 57600

#define enable_rs485pin 2
#define switch0 3
#define switch1 4
#define switch2 5
#define switch3 6
#define switch4 7
#define switch5 8
#define switch6 9
#define switch7 10
#define switch8 11
#define switch9 12
#define switch10 13
#define switch11 A0
#define switch12 A1
#define switch13 A2
#define switch14 A3
#define switch15 A4

#define flush_serial while(Serial.available()){Serial.read();}

/* This section is to make a place to jump to
 *  if we need to run the bootloader to update
 */
typedef void (*do_reboot_t)(void);
const do_reboot_t do_reboot = (do_reboot_t)((FLASHEND-511)>>1);

#define lighton LOW
#define lightoff HIGH

byte incomingByte;
byte command[6];
int i;

void jump_to_bootloader(){

  // Bootloader assumes a few things we need to setup before jumping.

  noInterrupts();
  MCUSR=0;
  do_reboot();  // jump
  
 
}

void setup()
{

  wdt_disable();
  interrupts();

  for (uint8_t i=0;i<3;i++)
  {
    digitalWrite(13, HIGH);
    send_byte_out((byte)0,0x01);
    send_byte_out((byte)1,0x00);
    delay(1000);

    send_byte_out((byte)0,0x00);
    send_byte_out((byte)1,0x00);        
    digitalWrite(13, LOW);   
    delay(200); 
  }

  // Enable RS485 chip
  pinMode(enable_rs485pin, OUTPUT);
  digitalWrite(enable_rs485pin, LOW);  // receive = low
  
  pinMode(switch0, OUTPUT);
  pinMode(switch1, OUTPUT);
  pinMode(switch2, OUTPUT);
  pinMode(switch3, OUTPUT);
  pinMode(switch4, OUTPUT);
  pinMode(switch5, OUTPUT);
  pinMode(switch6, OUTPUT);
  pinMode(switch7, OUTPUT);

  pinMode(switch8, OUTPUT);
  pinMode(switch9, OUTPUT);
  pinMode(switch10, OUTPUT);
  pinMode(switch11, OUTPUT);
  pinMode(switch12, OUTPUT);
  pinMode(switch13, OUTPUT);
  pinMode(switch14, OUTPUT);
  pinMode(switch15, OUTPUT);

  // Turn them all off
  digitalWrite(switch0, lightoff);
  digitalWrite(switch1, lightoff);
  digitalWrite(switch2, lightoff);
  digitalWrite(switch3, lightoff);
  digitalWrite(switch4, lightoff);
  digitalWrite(switch5, lightoff);
  digitalWrite(switch6, lightoff);
  digitalWrite(switch7, lightoff);

  digitalWrite(switch8, lightoff);
  digitalWrite(switch9, lightoff);
  digitalWrite(switch10, lightoff);
  digitalWrite(switch11, lightoff);
  digitalWrite(switch12, lightoff);
  digitalWrite(switch13, lightoff);
  digitalWrite(switch14, lightoff);
  digitalWrite(switch15, lightoff);

  /*
   *  Each word at 9600 baud takes 1.042ms to send
   *  That means four bytes takes ~4ms to transmit.
   *  We'll wait 25 ms between transmissions
   */
  Serial.begin(BAUD_RATE);
  Serial.setTimeout(15);

}

void send_byte_out(byte bank, byte incomingByte){

    if (bank==0)
    {

      if (incomingByte & (1<<0))
         digitalWrite(switch0, lighton);
      else
        digitalWrite(switch0, lightoff);
  
      if (incomingByte & (1<<1))
         digitalWrite(switch1, lighton);
      else
        digitalWrite(switch1, lightoff);
  
      if (incomingByte & (1<<2))
         digitalWrite(switch2, lighton);
      else
        digitalWrite(switch2, lightoff);
  
      if (incomingByte & (1<<3))
         digitalWrite(switch3, lighton);
      else
        digitalWrite(switch3, lightoff);
  
      if (incomingByte & (1<<4))
         digitalWrite(switch4, lighton);
      else
        digitalWrite(switch4, lightoff);
  
      if (incomingByte & (1<<5))
         digitalWrite(switch5, lighton);
      else
        digitalWrite(switch5, lightoff);
  
      if (incomingByte & (1<<6))
         digitalWrite(switch6, lighton);
      else
        digitalWrite(switch6, lightoff);
  
      if (incomingByte & (1<<7))
         digitalWrite(switch7, lighton);
      else
        digitalWrite(switch7, lightoff);
    }

    if (bank==1)
    {

      if (incomingByte & (1<<0))
         digitalWrite(switch8, lighton);
      else
        digitalWrite(switch8, lightoff);
  
      if (incomingByte & (1<<1))
         digitalWrite(switch9, lighton);
      else
        digitalWrite(switch9, lightoff);
  
      if (incomingByte & (1<<2))
         digitalWrite(switch10, lighton);
      else
        digitalWrite(switch10, lightoff);
  
      if (incomingByte & (1<<3))
         digitalWrite(switch11, lighton);
      else
        digitalWrite(switch11, lightoff);
  
      if (incomingByte & (1<<4))
         digitalWrite(switch12, lighton);
      else
        digitalWrite(switch12, lightoff);
  
      if (incomingByte & (1<<5))
         digitalWrite(switch13, lighton);
      else
        digitalWrite(switch13, lightoff);
  
      if (incomingByte & (1<<6))
         digitalWrite(switch14, lighton);
      else
        digitalWrite(switch14, lightoff);
  
      if (incomingByte & (1<<7))
         digitalWrite(switch15, lighton);
      else
        digitalWrite(switch15, lightoff);
    }
}


void loop()
{
  network_mode();
  //night_rider();
}


void blank_tree()
{
  send_byte_out((byte)0,0);
  send_byte_out((byte)1,0);
}

void spin_tree(byte slices_to_run_for, byte dir, byte cummulative)
{
  unsigned long delay_value;
  int num_steps=16;
  word tree_pattern; // int is 16bit so it can store whole tree.

  if (dir==0)
    tree_pattern=1;
  else
    tree_pattern=0b1000000000000000;
  

  delay_value = slices_to_run_for*1000/SLICES_PER_SECOND/(num_steps);

  for (int i=0;i<num_steps;i++)
  {
    send_byte_out((byte)0,(byte)tree_pattern);
    send_byte_out((byte)1,(byte)(tree_pattern >> 8));
    delay(delay_value);

    if (dir==0)
      if (cummulative==0)
        tree_pattern <<= 1;
      else
        tree_pattern |= tree_pattern << 1;
    else  // dir must not be zero, going the other way then
      if (cummulative==0)
        tree_pattern >>= 1;
      else
        tree_pattern |= tree_pattern >> 1;
      
  }

  blank_tree();
}

void network_mode(void)
{

  flush_serial
  Serial.readBytes(command,6);

  // First check to see if someone else is getting programmed.  If so, turn off for a bit and flush the uart...
  // If this command isn't for us and it is a reset to bootloarder command
  if ( (command[0]!=arduino_address) && (command[1]==0x00) && (command[2]==0x05) )
  {

    send_byte_out((byte)0,0x01);
    send_byte_out((byte)1,0x00);
    
    digitalWrite(13, HIGH);
    delay(20000);  // Wait for all programming to finish
    while(Serial.available()){Serial.read();}  // Clear any garbage out
    digitalWrite(13, LOW);
    send_byte_out((byte)0,0x01);  // Turn on the first light to show we're sleeping
    send_byte_out((byte)1,0x00);
    
    command[0]=0xfe; // Make this something other than 0 for when we start so we don't just go to bootloader... 
    command[1]=0xfe;
  }
  else if ( (command[0]==0x00) || (command[0]==arduino_address) )  // If it's a broadcast or just to me...
  {

    // The command is to me or "us"...
    switch (command[1])
    {

      case 0x00:  // Control command?
      {
        // Reboot to bootloader to program
        // 0x02 0x00 0x05 0x00 0x00 0x00
        if (command[2] == 0x05)  
        {
          send_byte_out((byte)0,0x02);  // Turn on second light to show we're rebooted into programming mode
          send_byte_out((byte)1,0x00);
          jump_to_bootloader();
          break;
        }

        // Blank all lights
        // 0x02 0x01 0x00 0x00 0x00 0x00
        if (command[2] == 0x01)  // Blank
        {
          send_byte_out((byte)0,0x00);
          send_byte_out((byte)1,0x00);
          break;
        }

        break;  // Done with the control mode
      } // control command
        
      // Send two bytes directly
      // 0x02 0x01 0xF0 0xF0 0x00 0x00
      case 0x01:  
      {   
        send_byte_out((byte)0,command[2]);
        send_byte_out((byte)1,command[3]);
        break;
      }

      // Spin around the tree for command[2] 1/10 seconds at command[3] dir... and command[4] cummulative
      // 0x02 0x02 0x14 0x01 0x00 0x00
      case 0x02:
      {
        spin_tree(command[2],command[3],command[4]);
        break;
      }
      
    }
  }

  command[0]=0xfe;
}


void night_rider()
{
  static int dir=0;
  
  send_byte_out((byte)0,command[1]);
  send_byte_out((byte)1,command[2]);
  delay(100);
  if (dir == 0)
    command[1] <<= 1;
  else
    command[1] >>= 1;

 if (command[1]==0b10000000)
  dir = 1;
if (command[1]== 0b00000001)
  dir = 0;
  
}



