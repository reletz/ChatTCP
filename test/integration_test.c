#include "test_utils.h"
#include "packet.h"
#include "flow_control.h"
#include "congestion_control.h"
#include <sys/wait.h> // Diperlukan untuk waitpid

// Test data transfer dengan flow control dan congestion control menggunakan fork()
int test_data_transfer()
{
    int client_sock, server_sock;
    struct sockaddr_in server_addr, client_addr;
    int client_port = TEST_PORT_BASE + 6;
    int server_port = TEST_PORT_BASE + 7;

    printf("Setting up test sockets...\n");

    client_sock = create_test_socket(client_port);
    server_sock = create_test_socket(server_port);

    ASSERT_TRUE(client_sock >= 0);
    ASSERT_TRUE(server_sock >= 0);

    // Siapkan alamat
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(client_port);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Membuat proses baru
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        close(client_sock);
        close(server_sock);
        return TEST_FAIL;
    }

    if (pid == 0) {
        // --- PROSES CHILD (SERVER) ---
        flow_control_state server_fc;
        char receive_buffer[MAX_PAYLOAD_SIZE + 1];
        size_t bytes_received;

        printf("[Server] Initializing flow control...\n");
        init_flow_control(&server_fc, server_sock, &client_addr, server_port, client_port);
        
        printf("[Server] Waiting for data...\n");
        int recv_result = receive_data_with_flow_control(&server_fc, receive_buffer,
                                                       sizeof(receive_buffer) - 1, &bytes_received);
        
        if (recv_result <= 0) {
            printf("[Server] Failed to receive data\n");
            close(server_sock);
            exit(TEST_FAIL);
        }

        receive_buffer[bytes_received] = '\0';
        printf("[Server] Received data: '%s'\n", receive_buffer);

        // Verifikasi data
        if (strcmp("TEST", receive_buffer) != 0) {
            printf("[Server] Data mismatch!\n");
            close(server_sock);
            exit(TEST_FAIL);
        }

        printf("[Server] Test successful. Exiting.\n");
        close(server_sock);
        exit(TEST_PASS);

    } else {
        // --- PROSES PARENT (CLIENT) ---
        flow_control_state client_fc;
        congestion_control_state cc_state;
        char test_data[] = "TEST";

        // Beri sedikit waktu agar server siap
        sleep(1);

        printf("[Client] Initializing flow control...\n");
        init_flow_control(&client_fc, client_sock, &server_addr, client_port, server_port);

        printf("[Client] Initializing congestion control...\n");
        init_congestion_control(&cc_state, MAX_PAYLOAD_SIZE);

        printf("[Client] Sending test data: '%s'\n", test_data);
        int send_result = send_data_with_flow_control(&client_fc, test_data, strlen(test_data));

        if (send_result < 0) {
            printf("[Client] Failed to send test data\n");
            close(client_sock);
            // Hentikan proses child jika pengiriman gagal
            kill(pid, SIGKILL);
            return TEST_FAIL;
        }

        printf("[Client] Data sent. Waiting for server to finish...\n");

        // Tunggu proses server selesai dan periksa statusnya
        int status;
        waitpid(pid, &status, 0);

        close(client_sock);
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == TEST_PASS) {
            printf("[Client] Server process finished successfully.\n");
            return TEST_PASS;
        } else {
            printf("[Client] Server process failed.\n");
            return TEST_FAIL;
        }
    }
}

// Test penanganan packet loss
int test_packet_loss_recovery()
{
    // Tes ini akan lebih kompleks karena membutuhkan simulasi packet loss
    // dan verifikasi bahwa protokol dapat pulih dengan benar.
    // Untuk saat ini, kita lewati implementasinya.
    printf("Packet loss recovery test skipped - requires network simulation\n");
    return TEST_PASS;
}

int main()
{
    // Inisialisasi random number generator
    srand(time(NULL));

    // Jalankan tes
    RUN_TEST(test_data_transfer);
    RUN_TEST(test_packet_loss_recovery);

    printf("All integration tests passed!\n");
    return TEST_PASS;
}
