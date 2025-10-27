#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

// ★ 1. 変更点: 実際のCANインターフェース名に変更してください
// (例: "can0", "slcan0" など)
#define CAN_NAME "can0"

int main(int argc, char* argv[])
{
    int s; // CANソケットのファイルディスクリプタ
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    int nbytes;

    // --- ソケットの初期化 (プログラム起動時に1回だけ実行) ---

    // 1. ソケットの作成
    if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        perror("socket");
        return 1;
    }

    // 2. インターフェース名を設定
    strncpy(ifr.ifr_name, CAN_NAME, sizeof(ifr.ifr_name) -1);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0'; // 念のためヌル終端

    // 3. インターフェース名からインデックスを取得
    ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
    if(!ifr.ifr_ifindex)
    {
        // インターフェースが見つからない場合のエラー
        fprintf(stderr, "Error: CAN interface '%s' not found./n", CAN_NAME);
        perror("if_nametoindex");
        close(s);
        return 1;
    }

    // 4. ソケットをインターフェースにバインド
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(s);
        return 1;
    }

    printf("CAN interface '%s' (ifindex %d) に接続しました。受信待機中...\n", CAN_NAME, ifr.ifr_ifindex);

    // --- ★ 2. 変更点: 受信ループ (継続的にデータを読み込む) ---
    while(1)
    {
        // 5. CANフレームの読み込む (データが来るまでブロックする)
        // 元のコードの select() はタイムアウト用でしたが、
        // read() だけでデータが来るまで待機する動作になります。
        nbytes = read(s, &frame, sizeof(struct can_frame));

        if(nbytes < 0)
        {
            perror("read"); // 読み込みエラー
            continue; // ループを続行
        }

        if(nbytes < sizeof(struct can_frame))
        {
            fprintf(stderr, "read: incomplete CAN frame\n");
            continue; // ループを続行
        }

        // --- ★ 2. 変更点: RoboMasterの解読ロジックをここに追加 ---
        if (frame.can_id == 0x201)
        {
            // Arduinoコードのロジックをそのまま移植
            // (short ではなく int16_t を使うと、どの環境でも16bitを保証できます)

            // 角度 [0-8191]
            u_int16_t angle_raw = (frame.data[0] << 8) | frame.data[1];

            // RPM (符号付き)
            int16_t rpm_raw = (frame.data[2] << 8) | frame.data[3];

            // トルク/電流 (符号付き)
            int16_t torque_raw = (frame.data[4] << 8) | frame.data[5];

            // 温度 (符号なし)
            u_int8_t temp_raw = frame.data[6];

            float current_A = ( (float)torque_raw / 16384.0f ) * 20.0f;
            float angle_deg = ( (float)angle_raw / 8191.0f ) * 360.0f;

            // ★ 3. 変更点: 解読した値を表示
            // \rは「行の先頭に戻る」という意味。
            // これにより、1行でデータが更新され続けます。
            printf("ID=0x201 | Angle: %6.1f ° | RPM: %6d | Torque: %6.2f A | Temp: %3u ℃ \r",
             angle_deg, rpm_raw, current_A, temp_raw);

             // \r で表示を更新するときは、fflush(stdout) が必要です
             fflush(stdout);
        }
        else
        {
            // 0x201 以外のIDが来たら、改行して通常通り表示
            printf("\nID=0x%03x DLC=%d Data=", frame.can_id & CAN_SFF_MASK,frame.can_dlc);
            for(int i = 0; i< frame.can_dlc; i++)
            {
                printf("%02x ", frame.data[i]);
            }
            printf("\n");
        }
    }
    // //     // ★ 3. 変更点: 受信したデータを表示
    // //     // RoboMasterモーター (M3508やM2006など) は、
    // //     // ID: 0x201 ~ 0x20B の範囲でフィードバック (電流、角度、速度) を返すはずです。

    // //     // 標準ID (11bit) のみ考慮 (EFFフラグをチェックしない)
    // //     printf("ID=0x%03X DLC=%d Data=", frame.can_id & CAN_SFF_MASK, frame.can_dlc);

    // //     for(int i = 0; i < frame.can_dlc; i++)
    // //     {
    // //         printf("%02X ", frame.data[i]);
    // //     }
    // //     printf("\n");
    // // }

    // // 6. ソケットを閉じる (while(1) なので通常は到達しない)
    // printf("ソケットを閉じます。 \n");
    close(s);
    return 0;



}



// int canrecv_stdframe(unsigned short *p_id, unsigned char *p_dlc, unsigned char data[])
// {
//     int ret;
//     int s;
//     fd_set rdfs;
//     struct ifreq ifr;
//     struct sockaddr_can addr;
//     struct timeval timeout;
//     struct can_frame frame;
//     int nbytes;

//     if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
//     {
//         perror("socket");
//         return -1;
//     }

//     memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
//     strncpy(ifr.ifr_name, CAN_NAME, sizeof(ifr.ifr_name));

//     ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
//     if(! ifr.ifr_ifindex)
//     {
//         perror("if_nametoindex");
//         return -2;
//     }

//     addr.can_family = AF_CAN;
//     addr.can_ifindex = ifr.ifr_ifindex;

//     if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
//     {
//         perror("bind");
//         return -3;
//     }

//     while(1)
//     {
//         FD_ZERO(&rdfs);
//         FD_SET(s, &rdfs);

//         timeout.tv_sec = 1;
//         timeout.tv_usec = 0;

//         ret = select(s+1, &rdfs, NULL, NULL, &timeout);
//         if(ret < 0)
//         {
//             perror("select");
//             return -4;
//         }
//         else if(0 == ret)
//         {
//             continue;
//         }
//         else
//         {
//             break;
//         }
//     }

//     nbytes = read(s, &frame, sizeof(frame));
//     if(nbytes < 0)
//     {
//         perror("recv");
//         return -5;
//     }

//     if(nbytes == sizeof(frame))
//     {
//         *p_id = frame.can_id;
//         *p_dlc = frame.can_dlc;
//         memcpy(data, frame.data, CAN_MAX_DLEN);
//     }
//     else
//     {
//         fprintf(stderr, "recv size not std-frame.\n");
//     }

//     close(s);

//     return 0;

// }

// int main(int argc, char* argv[])
// {
//     unsigned short id;
//     unsigned char dlc;
//     unsigned char data[CAN_MAX_DLEN];

//     canrecv_stdframe(&id, &dlc, data);

//     printf("CAN interface '%s' (ifindex %d) に接続しました。受信待機中...\n", CAN_TNTERFACE, ifr.ifrifindex);


//     printf("id=%x, dlc=%d, data=%.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x\n", id, dlc, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
// }



// {
//  printf("%02x ", frame.data[i]);
// }
//  printf("\n"); 
// }
//  // 6. ソケットを閉じる (while(1) なので通常は到達しない)
//  printf("ソケットを閉じます。\n");
//  close(s);
//  return 0;
// }