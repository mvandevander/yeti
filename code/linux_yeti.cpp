#include <stdio.h>
#include <xcb/xcb.h>
#include <stdlib.h>
#include <X11/keysym.h>

#include <xcb/xcb_keysyms.h>
#include <assert.h>

#define SET_BIT(byte, bit_to_set) (byte |= (1 << (bit_to_set)))
#define CLEAR_BIT(byte, bit_to_set) (byte &= ~(1 << (bit_to_set)))
#define TEST_BIT(byte, bit_to_set) ((byte & (1 << (bit_to_set))) != 0)

#define bool32_t int32_t

uint32_t GetUTF8EncodedWidth(uint32_t code_point)
{
    uint32_t result = 0;
    if(code_point <= 0x7F)
    {
        // NOTE(matthew): Technically this means that what we have submitted
        // is already a valid UTF-8 encoded character. So perhaps we want to
        // consider returning right here instead?
        result = 1;
    }
    else if(code_point <= 0x07FF)
    {
        result = 2;
    }
    else if(code_point <= 0xFFFF)
    {
        result = 3;
    }
    /*else if(code_point > 0xD800 and code_point < 0xDFFF)
    {
        // NOTE(matthew): Invalid code points according to standard
    }*/
    else // code_point max value is 0x1FFFFF
    {
        // NOTE(matthew): Seems like the highest usable code point may
        // actually be 0x10FFFD? Wikipedia says the safe end point is
        // 0x10FFFF?
        result  = 4;
    }
    return result;
}

bool EncodeUTF8(uint32_t code_point, char* result, int utf8_width)
{
    if(utf8_width == 0 or result == NULL) //error with function input
    {
        // TODO(matthew): Log the error
        return false;
    }
    else if(code_point > 0xD800 and code_point < 0xDFFF)
    {
        // NOTE(matthew): Invalid code points according to standard
        result = NULL;
        return false;
    }
    
    if(utf8_width == 1)
    {
        // NOTE(matthew): Whatever the user submitted was an ASCII character,
        // which is already a valid UTF-8 encoded character, so we return
        // early
        result[0] = code_point;
        return true;
    }
    
    char byte;
    int available_bits_used = 0;

    for(int index = 0; index < utf8_width; index++)
    {
        // NOTE(matthew): This is a temporary store for the current operating
        // byte's contents. 
        // TODO(matthew): Perhaps think about removing the
        // temporary variable and just operating on result indicies directly?
        byte = 0;
        
        // NOTE(matthew): EXPLANATION OF ALGORITHM: For each output byte, we
        // fill all available bits with the corresponding bits from the code
        // point. In order to do this, we keep track of the
        // available_bits_used and then shift the code point value down by the
        // number of bits we have used. We then set the high bits as specified
        // by the encoding standard. 10 for all bytes except the MSB, which
        // will have high order 1's equal to the utf8_width
        
        byte = code_point >> available_bits_used;
        
        if(index < (utf8_width - 1))
        {
            SET_BIT(byte, 7);
            CLEAR_BIT(byte, 6);
            available_bits_used += 6;
        }
        else
        {
            // NOTE(matthew): This is the Most Significant Byte
            for(int index2 = 0; index2 < utf8_width; index2++)
            {
                SET_BIT(byte, 7-index2);
            }
            CLEAR_BIT(byte, 7-utf8_width);
        }
        result[(utf8_width - 1) - index] = byte;
    }
    return true;
}

uint32_t DecodeUTF8(char* encoded_point_buffer) 
{
    // TODO(matthew): Return an error if the code point could not be decoded properly
    
    
    // NOTE(matthew): We don't need to know the width here, but it may be
    // faster to have another function that takes it if we know it?
    
    uint32_t result = 0;
    if(encoded_point_buffer == NULL) //something is wrong and the buffer is bad
    {
        // TODO(matthew): Logging
        return result;
    }
    
    // read the first byte and find the size
    char byte = encoded_point_buffer[0];
    int bits_on = 0;
    for(bits_on = 0; bits_on < 5; ++bits_on)
    {
        // NOTE(matthew): TEST_BIT may not work properly
        if(TEST_BIT(byte,7-bits_on) == false)
        {
            //we have hit a zero, and we should have the count now
            
            break;
        }
    }
    int utf8_width = bits_on;
    if(bits_on == 1) 
    {
        // NOTE(matthew): Only the top bit was on in the leading byte,
        // something is wrong, the code point we were passed is invalid?
        
        // TODO(matthew): Decide how to handle an invalid UTF-8 character
        // decode, perhaps we need a special error code return?
        return 0;
    }
    if(bits_on == 0) 
    {
        // NOTE(matthew): We came out of the loop on the first bit being zero,
        // so the width will be one, which means we can go ahead and extract
        // the byte early
        utf8_width = 1;
        
        
    }
    
    byte = 0;
    int available_bits_used = 0;
    
    if(utf8_width == 1)
    {
        // NOTE(matthew): Special case handle single byte wide characters
        byte = encoded_point_buffer[0];
        return byte;
    }
    int index_of_last_byte = utf8_width-1;
    for(int index = 0; index <= index_of_last_byte; ++index)
    {
        byte = encoded_point_buffer[index_of_last_byte-index];
        // check for invalid bytes
        
        if(index == index_of_last_byte)
        {
            for(int i = 0; i < 7-utf8_width; i++)
            {
                if(TEST_BIT(byte,i) == true)
                {
                    SET_BIT(result, i+available_bits_used);
                }
            }
            break;
        }
        else
        {
            if((TEST_BIT(byte,7) != 1) or (TEST_BIT(byte,6) != 0))
            {
                // NOTE(matthew): Byte did not have leading bits set properly, error
                return 0;
            }
            for(int i = 0; i < 6; i++)
            {
                if(TEST_BIT(byte,i) == true)
                {
                    SET_BIT(result, i+available_bits_used);
                }
            }
            available_bits_used+=6;
        }
    }
    return result;

}

int main(int argc, char **argv)
{

	xcb_screen_t *screen  = NULL;
	int screen_number;
	xcb_connection_t *connection;
	xcb_screen_iterator_t screen_iterator;

	connection = xcb_connect(NULL,&screen_number);
	
	if(!xcb_connection_has_error(connection))
	{
        const xcb_setup_t *xcb_setup_info = xcb_get_setup(connection);
        screen_iterator = xcb_setup_roots_iterator(xcb_setup_info);
		//TODO: Probably remove this for loop as I don't think
		//that it is entirely necessary. Instead we could probably
		//just get screen_iterator.data right here. Hard to tell
		//what it would do with multiple "screens" or what that
		//really even means.
        for(; screen_iterator.rem; --screen_number)
        {
         if(screen_number == 0)
         {
            screen = screen_iterator.data;
            break;
        }
    }
    printf("\n");
    printf("Informations of screen %d: \n", screen->root);
    printf("  width........: %d\n", screen->width_in_pixels);
    printf("  height.......: %d\n", screen->height_in_pixels);
    printf("  white pixel..: %d\n", screen->white_pixel);
    printf("  black pixel..: %d\n", screen->black_pixel);

    xcb_rectangle_t rectangles[] = {
     { 10, 50, 40, 20},
     { 80, 50, 10, 40}};
     xcb_drawable_t foreground_brush = screen->root;
     xcb_gcontext_t foreground_graphics_context = 
     xcb_generate_id(connection);
     uint32_t mask =	 XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
     uint32_t values[2];
     values[0] = screen->white_pixel;
     values[1] = 0; 
     xcb_create_gc(connection, foreground_graphics_context,
       foreground_brush, mask, values);
     

		//NOTE(matthew): We are using the same mask and
		//values variables that we used for creating
		//the graphics context, but the state is not
		//important. They are just throwaway variables
		//to pass to xcb functions.
		//TODO(matthew): Actually, probably good to make
		//sure that these go out of scope sometime.
     xcb_window_t window = xcb_generate_id(connection);
     mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
     values[0] = screen->black_pixel;
     values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
     xcb_create_window(connection,
      XCB_COPY_FROM_PARENT,
      window,
      screen->root,
      0,
      0,
      screen->width_in_pixels/2,
      screen->height_in_pixels/2,
      10,
      XCB_WINDOW_CLASS_INPUT_OUTPUT,
      screen->root_visual,
      mask,values); 

     const char* window_title = "yETI";
     xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window,
        XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 
        sizeof(window_title),
        window_title);
     xcb_map_window (connection, window);
     bool app_is_running = true;
     xcb_generic_event_t *event;
     xcb_flush(connection);
     
     
        // NOTE(matthew): This sets the repeat mode, currently it does nothing
        // because it is using XCB_AUTO_REPEAT_MODE_DEFAULT, however by using
        // another settings like XCB_AUTO_REPEAT_MODE_OFF, we can change the
        // setting. Bear in mind that this is a system-wide setting. So
        // changing it may not exactly be desirable.
     uint32_t keyboard_control_values[1];
     keyboard_control_values[0] = XCB_AUTO_REPEAT_MODE_DEFAULT;
     xcb_change_keyboard_control(connection, XCB_KB_AUTO_REPEAT_MODE, keyboard_control_values);
     FILE *file = fopen("./blah.txt","w");
        // NOTE(matthew): Seems like the highest usable code point is actually 0x10FFFD?
     for(int code_point = 0; code_point <= 0x10FFFD; code_point++)
     {
        int utf8_width = GetUTF8EncodedWidth(code_point);
        char* encoded_point_buffer = (char*)calloc(1,utf8_width);
        if(!EncodeUTF8(code_point, encoded_point_buffer, utf8_width))
        {
            continue;
        }
        uint32_t decoded_utf8_code_point = DecodeUTF8(encoded_point_buffer);
        free(encoded_point_buffer);
        utf8_width = GetUTF8EncodedWidth(decoded_utf8_code_point);
        encoded_point_buffer = (char*)calloc(1,utf8_width);
        EncodeUTF8(decoded_utf8_code_point, encoded_point_buffer, utf8_width);
        //assert(code_point == decoded_utf8_code_point);
        if(encoded_point_buffer != NULL) //If the encoded point is void, then we will be unable to encode it
        {
            fprintf(file, "%s",encoded_point_buffer);
            if(code_point % 100 == 0)
            {
                fprintf(file, "\n");
            }
            free(encoded_point_buffer);
        }
    }
    fclose(file);
    fflush(stdout);
    return 0;
    
    while(app_is_running and (event = xcb_wait_for_event(connection)))
    {
    			//printf("running\n");
    			//printf("processing event");
         if(event->response_type == XCB_EXPOSE)
         {
            printf("Expose event\n");
            xcb_poly_rectangle(connection, window,
              foreground_graphics_context,
              2, rectangles);
            xcb_flush(connection);

        }
        else if(event->response_type == XCB_MAPPING_NOTIFY)
        {
            
        }
        else if(event->response_type == XCB_KEY_PRESS)
        {
            
            xcb_key_press_event_t *key_press_event = (xcb_key_press_event_t *)event;
            xcb_keycode_t keycode = key_press_event->detail;
    		//NOTE: We are passing 1 to the return keysym
    		//count, but we may need to get more
    		//There is probably a way to get the max
    		//required?
            xcb_get_keyboard_mapping_cookie_t cookie =
            xcb_get_keyboard_mapping(connection,
              keycode, 1);
            xcb_generic_error_t *error;
            xcb_get_keyboard_mapping_reply_t *reply =
            xcb_get_keyboard_mapping_reply(connection,
                                           cookie,
                                           &error);
            xcb_keysym_t *keysyms = xcb_get_keyboard_mapping_keysyms(reply);
            // NOTE(matthew): If the user pressed the escape key then
            // we close the appllication, just temporary.
            if(keysyms[0] == XK_Escape)
            {
               app_is_running = false;
            }
            //setlocale(LC_ALL,"");
            uint16_t letter = (keysyms[0]);
            if(letter == XK_Shift_L or letter == XK_Shift_R)
            {
                letter = 0;
            }
            if(key_press_event->state & XCB_MOD_MASK_SHIFT)
            {
                letter = keysyms[1];
            }
            //TODO(matthew): Make this allocation less horrifying and probably dangerous
             //char *keysym_buffer = (char*)(malloc(8));
             //xkb_keysym_to_utf8((xkb_keysym_t)keysyms[0], keysym_buffer, 8);
             //const char *letter2 ="ト"; 
            char letter2[2];
            *letter2 = *(&keysyms[0]);
            printf("%s", (char*)(&keysyms[0]));
            fflush(stdout);
    		//printf("チトシハKeyPress recieved: %c (%d)\n",letter,letter);
            //printf("XKB says: %s\n",keysym_buffer);
        
           // free(keysym_buffer);
        
    		//app_is_running = false;
        }
        
		// TODO(matthew): Use a different non stdlib memory freeing function. 
		// Though needing to allocate for a damn event is retarded.
        free(event);

    }

    xcb_disconnect(connection);
    }
    else
    {
    	//TODO: Error handing
    }
    return 0;
}

