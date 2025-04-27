#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <driver/adc.h>

#include "ssd1306.h"
#include "font8x8_basic.h"

#define GPIO_JOYSTICK_SW 14
#define JOYSTICK_X_DEFAULT 1835
#define JOYSTICK_Y_DEFAULT  1790
#define JOYSTICK_Y_DEFAULT_OFFSET  100
#define VRX_PIN  36 // ESP32 pin GPIO36 (ADC0) connected to VRX pin
#define VRY_PIN  39 // ESP32 pin GPIO39 (ADC0) connected to VRY pin

#define PLAYER1_X_FIXED 5
#define PLAYER2_X_FIXED 108
#define PLAYERS_Y_DEFAULT 22
#define PLAYER_TOP_BORDER 0
#define PLAYER_BOTTOM_BORDER 43
#define PLAYERS_X_SIZE 8
#define PLAYERS_Y_SIZE 20

#define BALL_X_DEFAULT 64
#define BALL_Y_DEFAULT 32
#define BALL_SIZE 8

#define SPEED_1 1
#define SPEED_2 2
#define SPEED_2_THRESHOLD 850
#define SPEED_3 3
#define SPEED_3_THRESHOLD 1400

#define TAG "SSD1306"

struct game_informations{
    int player1_posX;       // Player 1 X position
    int player1_posY;       // Player 1 Y position

    int player2_posX;       // Player 2 X position
    int player2_posY;       // Player 2 Y position

    int ball_posX;          // Ball X position
    int ball_posY;          // Ball Y position
    int ball_dirX;          // Ball X direction
    int ball_dirY;          // Ball Y direction

    bool player1_on_turn;
};


uint8_t ball_image1[8] = {
    0xff,
    0x99,
    0xa5,
    0xdb,
    0xdb,
    0xa5,
    0x99,
    0xff
};

uint8_t player_image1[20] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
};

uint8_t empty_image[8] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};

uint8_t empty_image_player[20] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/**
 * @brief Sets default values to members of structure game_information
 *
 * @param game_info structure to be set
 */
void init_game_info_struct(struct game_informations *game_info){
    game_info->player1_posX = PLAYER1_X_FIXED;
    game_info->player1_posY = PLAYERS_Y_DEFAULT;

    game_info->player2_posX = PLAYER2_X_FIXED;
    game_info->player2_posY = PLAYERS_Y_DEFAULT;

    game_info->ball_posX = BALL_X_DEFAULT;
    game_info->ball_posY = BALL_Y_DEFAULT;
    game_info->ball_dirX = 1;
    game_info->ball_dirY = 1;

    game_info->player1_on_turn = false;
}


/**
 * @brief Calculates new Y value of the player and moves the player on the field
 *
 * @param dev display device
 * @param player_posY current Y position of the player
 * @param player_posX current X position of the player
 * @return new Y value of the moved player
 */
int move_player(SSD1306_t *dev, int player_posY, int player_posX){
    // Get analog Y input value from Joystick
    int valueY = adc1_get_raw(ADC1_CHANNEL_3);

    if (valueY < JOYSTICK_Y_DEFAULT - JOYSTICK_Y_DEFAULT_OFFSET){
        // Player is moving up
        if (player_posY > PLAYER_TOP_BORDER){
            int y_shift;
            if (valueY < JOYSTICK_Y_DEFAULT - SPEED_3_THRESHOLD){
                y_shift = SPEED_3;
            }
            else if (valueY < JOYSTICK_Y_DEFAULT - SPEED_2_THRESHOLD){
                y_shift = SPEED_2;
            }
            else {
                y_shift = SPEED_1;
            }

            // Render new position of the player
            ssd1306_bitmaps(dev, player_posX, player_posY, empty_image_player, PLAYERS_X_SIZE, PLAYERS_Y_SIZE, false);
            player_posY -= y_shift;
            ssd1306_bitmaps(dev, player_posX, player_posY, player_image1, PLAYERS_X_SIZE, PLAYERS_Y_SIZE, false);
        }
    }
    else if (valueY > JOYSTICK_Y_DEFAULT + JOYSTICK_Y_DEFAULT_OFFSET){
        // Player is moving down
        if (player_posY < PLAYER_BOTTOM_BORDER){
            int y_shift;
            if (valueY > JOYSTICK_Y_DEFAULT + SPEED_3_THRESHOLD){
                y_shift = SPEED_3;
            }
            else if (valueY > JOYSTICK_Y_DEFAULT + SPEED_2_THRESHOLD){
                y_shift = SPEED_2;
            }
            else {
                y_shift = SPEED_1;
            }

            // Render new position of the player
            ssd1306_bitmaps(dev, player_posX, player_posY, empty_image_player, PLAYERS_X_SIZE, PLAYERS_Y_SIZE, false);
            player_posY += y_shift;
            ssd1306_bitmaps(dev, player_posX, player_posY, player_image1, PLAYERS_X_SIZE, PLAYERS_Y_SIZE, false);
        }
    }

    return player_posY;
}


/**
 * @brief Initializes peripherals
 *
 * @param dev display device to be initialized
 */
void init_device(SSD1306_t *dev){
    ESP_ERROR_CHECK(nvs_flash_init());

    // ADC configuration
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC1_CHANNEL_3,ADC_ATTEN_DB_11);

    // Joystick button configuration
    esp_rom_gpio_pad_select_gpio(GPIO_JOYSTICK_SW);
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_JOYSTICK_SW, GPIO_MODE_INPUT));

    // I2C display configuration
    i2c_master_init(dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(dev, 128, 64);
}


/**
 * @brief Displays the init screen
 *
 * @param dev display device
 */
void start_device_screen(SSD1306_t *dev){
    ssd1306_clear_screen(dev, false);
	ssd1306_display_text_x3(dev, 0, "IMP", 3, false);
    ssd1306_display_text(dev, 3, "Project No.8", 12, false);
    ssd1306_display_text(dev, 6, "xkrick01   2023", 15, false);
	vTaskDelay(5000 / portTICK_PERIOD_MS);
}


/**
 * @brief Displays idle screen and waits until the games is started (button is pushed)
 *
 * @param dev display device
 */
void press_button_screen(SSD1306_t *dev){
    ssd1306_clear_screen(dev, false);
    ssd1306_contrast(dev, 0xff);
    ssd1306_display_text(dev, 2, "Press the button", 16, false);
    ssd1306_display_text(dev, 4, "to start game!", 14, false);

    // Wait for Joystick button to be pressed
    while(gpio_get_level(GPIO_JOYSTICK_SW)){
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


/**
 * @brief Renders all game field components (players and game field lines)
 *
 * @param dev display device
 * @param game_info structure with all game informations
 */
void render_field(SSD1306_t *dev, struct game_informations *game_info){
    ssd1306_clear_screen(dev, false);

    // Render game field lines
    _ssd1306_line(dev, 0, 0, 127, 0, false);
    _ssd1306_line(dev, 0, 63, 127, 63, false);
    _ssd1306_line(dev, 63, 0, 63, 13, false);
    _ssd1306_line(dev, 64, 0, 64, 13, false);
    _ssd1306_line(dev, 63, 43, 63, 63, false);
    _ssd1306_line(dev, 64, 43, 64, 63, false);

    // Render players
    ssd1306_bitmaps(dev, game_info->player1_posX, game_info->player1_posY, player_image1, PLAYERS_X_SIZE, PLAYERS_Y_SIZE, false);
    ssd1306_bitmaps(dev, game_info->player2_posX, game_info->player2_posY, player_image1, PLAYERS_X_SIZE, PLAYERS_Y_SIZE, false);

    ssd1306_show_buffer(dev);
}


/**
 * @brief Counts down the start of the game and diplays time left on the screen
 *
 * @param dev display device
 */
void count_down(SSD1306_t *dev){

    // Render count down
    for (int font=0x33;font>0x30;font--){
        ssd1306_display_image(dev, 3, 60, font8x8_basic_tr[font], 8);
        vTaskDelay(750 / portTICK_PERIOD_MS);
    }
    ssd1306_display_image(dev, 3, 60, empty_image, 8);


    // Rerender middle line after countdown
    _ssd1306_line(dev, 63, 0, 63, 63, false);
    _ssd1306_line(dev, 64, 0, 64, 63, false);
}


/**
 * @brief Logs ball and players position, when a ball collide with player or when game over
 *
 * @param game_info structure with all game informations
 * @param p1 log player 1 when True or log player 2 when False
 * @param game_over log game over when True or log ball-player when False
 */
void log_game_state(struct game_informations *game_info, bool p1, bool game_over){
    if (p1 && !game_over){
        ESP_LOGI(TAG, "Ping: Ball - X=%d, Y=%d,  Player1 - X=%d, Y=%d", game_info->ball_posX, game_info->ball_posY, game_info->player1_posX, game_info->player1_posY);
    }
    else if(p1 && game_over){
        ESP_LOGI(TAG, "P1 Game Over: Ball - X=%d, Y=%d,  Player1 - X=%d, Y=%d", game_info->ball_posX, game_info->ball_posY, game_info->player1_posX, game_info->player1_posY);
    }
    else if(!p1 && !game_over){
        ESP_LOGI(TAG, "Pong: Ball - X=%d, Y=%d,  Player2 - X=%d, Y=%d", game_info->ball_posX, game_info->ball_posY, game_info->player2_posX, game_info->player2_posY);
    }
    else{
        ESP_LOGI(TAG, "P2 Game Over: Ball - X=%d, Y=%d,  Player2 - X=%d, Y=%d", game_info->ball_posX, game_info->ball_posY, game_info->player2_posX, game_info->player2_posY);
    }
}


/**
 * @brief Checks and sets what player is on turn
 *
 * @param game_info structure with all game informations
 */
void check_player_on_turn(struct game_informations *game_info){
    if(game_info->ball_dirX > 0){
        // Ball is moving right
        game_info->player1_on_turn = false;
    }
    else{
        // Ball is moving left
        game_info->player1_on_turn = true;
    }
}


/**
 * @brief Checks and handles a ball collision with player and game over
 *
 * @param game_info structure with all game informations
 * @return 0, if game continue or 1, if game over
 */
int check_ball_player_collision(struct game_informations *game_info){
    if (game_info->ball_posX <= game_info->player1_posX + PLAYERS_X_SIZE && game_info->ball_dirX < 0){
        // Ball has reached X position of player1
        if ((game_info->player1_posY <= game_info->ball_posY + BALL_SIZE) && (game_info->player1_posY + PLAYERS_Y_SIZE >= game_info->ball_posY)){
            // Player1 hit the ball
            log_game_state(game_info, true, false);
            game_info->ball_dirX *= (-1);
        }
        else{
            // Game over
            log_game_state(game_info, true, true);
            return 1;
        }
    }
    else if ((game_info->ball_posX + BALL_SIZE) >= game_info->player2_posX && game_info->ball_dirX > 0){
        // Ball has reached X position of player2
        if (game_info->player2_posY <= game_info->ball_posY + BALL_SIZE && game_info->player2_posY + PLAYERS_Y_SIZE >= game_info->ball_posY){
            // Player1 hit the ball
            log_game_state(game_info, false, false);
            game_info->ball_dirX *= (-1);
        }
        else{
            // Game over
            log_game_state(game_info, false, true);
            return 1;
        }
    }

    return 0;
}


/**
 * @brief Checks and handles a ball collision with border
 *
 * @param game_info structure with all game informations
 */
void check_ball_border_collision(struct game_informations *game_info){
    if (game_info->ball_posY <= 0 && game_info->ball_dirY < 0){
        // Ball has hit the top border
        game_info->ball_dirY *= (-1);
    }
    else if (game_info->ball_posY>= 56 && game_info->ball_dirY > 0){
        // Ball has hit the bottom border
        game_info->ball_dirY *= (-1);
    }
}


/**
 * @brief Moves the ball in set direction
 *
 * @param dev display device
 * @param game_info structure with all game informations
 */
void move_ball(SSD1306_t *dev, struct game_informations *game_info){
    ssd1306_bitmaps(dev, game_info->ball_posX, game_info->ball_posY, empty_image, BALL_SIZE, BALL_SIZE, false);

    game_info->ball_posX += game_info->ball_dirX;
    game_info->ball_posY += game_info->ball_dirY;

    ssd1306_bitmaps(dev, game_info->ball_posX, game_info->ball_posY, ball_image1, BALL_SIZE, BALL_SIZE, false);
}


/**
 * @brief Renders game field lines
 *
 * @param dev display device
 */
void rerender_field(SSD1306_t *dev){
     _ssd1306_line(dev, 0, 0, 127, 0, false);
    _ssd1306_line(dev, 0, 63, 127, 63, false);
    _ssd1306_line(dev, 63, 0, 63, 63, false);
    _ssd1306_line(dev, 64, 0, 64, 63, false);

    ssd1306_show_buffer(dev);
}


/**
 * @brief Handles the progress of the game
 *
 * @param dev display device
 * @param game_info structure with all game informations
 */
void game_logic(SSD1306_t *dev, struct game_informations *game_info){
    while(1) {
        check_player_on_turn(game_info);

        if (check_ball_player_collision(game_info)){
            break;
        }

        check_ball_border_collision(game_info);

        if (game_info->player1_on_turn){
            game_info->player1_posY = move_player(dev, game_info->player1_posY, game_info->player1_posX);
        }
        else{
            game_info->player2_posY = move_player(dev, game_info->player2_posY, game_info->player2_posX);
        }

        move_ball(dev, game_info);

        rerender_field(dev);
    }
}

void app_main() {
    SSD1306_t dev;

    init_device(&dev);

    start_device_screen(&dev);

    while(1){

        press_button_screen(&dev);

        struct game_informations game_info;
        init_game_info_struct(&game_info);

        render_field(&dev, &game_info);

        count_down(&dev);

        game_logic(&dev, &game_info);
    }
}