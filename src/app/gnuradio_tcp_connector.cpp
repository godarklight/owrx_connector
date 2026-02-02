#include "gnuradio_tcp_connector.hpp"
#include "owrx/gainspec.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <cstring>
#include <unistd.h>

int main (int argc, char** argv) {
    Connector* connector = new GNURadioTcpConnector();
    return connector->main(argc, argv);
}

uint32_t GNURadioTcpConnector::get_buffer_size() {
    return float_buffer_size;
}

std::stringstream GNURadioTcpConnector::get_usage_string() {
    std::stringstream s = Connector::get_usage_string();
    s << "";
    return s;
}

std::vector<struct option> GNURadioTcpConnector::getopt_long_options(){
    std::vector<struct option> long_options = Connector::getopt_long_options();
    return long_options;
}

int GNURadioTcpConnector::receive_option(int c, char* optarg) {
    switch (c) {
        case 'b':
            bias_tee = true;
            break;
        case 'e':
            direct_sampling = std::strtoul(optarg, NULL, 10);
            break;
        default:
            return Connector::receive_option(c, optarg);
    }
    return 0;
}

int GNURadioTcpConnector::parse_arguments(int argc, char** argv) {
    int r = Connector::parse_arguments(argc, argv);
    if (r != 0) return r;

    if (argc - optind >= 2) {
        host = std::string(argv[optind]);
        port = (uint16_t) strtoul(argv[optind + 1], NULL, 10);
    } else if (optind < argc) {
        std::string argument = std::string(argv[optind]);
        size_t colon_pos = argument.find(':');
        if (colon_pos == std::string::npos) {
            host = argument;
        } else {
            host = argument.substr(0, colon_pos);
            port = std::stoi(argument.substr(colon_pos + 1));
        }
    }

    return 0;
}

void GNURadioTcpConnector::print_version() {
    std::cout << "gnuradio_tcp_connector version " << VERSION << std::endl;
    Connector::print_version();
}

int GNURadioTcpConnector::open() {
    struct hostent* hp = gethostbyname(host.c_str());
    if (hp == NULL) {
        std::cerr << "gethostbyname() failed" << std::endl;
        return 3;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "float_tcp socket creation error: " << sock << std::endl;
        return 1;
    }

    struct sockaddr_in remote;

    std::memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr = *((struct in_addr *) hp->h_addr);

    if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        fprintf(stderr, "float_tcp connection failed\n");
        return 2;
    }
    return 0;
}

int GNURadioTcpConnector::send_command(struct command cmd) {
    ssize_t len = sizeof(cmd);
    ssize_t sent = send(sock, &cmd, len, 0);
    return len == sent ? 0 : -1;
}

int GNURadioTcpConnector::setup() {
    int r;

    r = Connector::setup();
    if (r != 0) return r;

    return 0;
}

int GNURadioTcpConnector::read() {
    ssize_t bytes_read;
    float_t* buf = (float_t*) malloc(float_buffer_size);

    while (run) {
        bytes_read = recv(sock, buf, float_buffer_size, 0);
        if (bytes_read > 0) {
            processSamples(buf, bytes_read / 4);
        } else {
            fprintf(stderr, "error or no data on float_tcp socket, closing connection\n");
            break;
        }
    }

    free(buf);

    return 0;
}

int GNURadioTcpConnector::close() {
    return ::close(sock);
}

void GNURadioTcpConnector::applyChange(std::string key, std::string value) {
    int r = 0;

    Connector::applyChange(key, value);
    return;

    if (r != 0) {
        std::cerr << "WARNING: setting \"" << key << "\" failed: " << r << std::endl;
    }
}

int GNURadioTcpConnector::set_center_frequency(double frequency) {
    return send_command((struct command) {0x01, htonl(frequency)});
}

int GNURadioTcpConnector::set_sample_rate(double sample_rate) {
    return send_command((struct command) {0x02, htonl(sample_rate)});
}

int GNURadioTcpConnector::set_gain(GainSpec* gain) {
    if (dynamic_cast<AutoGainSpec*>(gain) != nullptr) {
        return send_command((struct command) {0x03, htonl(0)});
    }

    SimpleGainSpec* simple_gain;
    if ((simple_gain = dynamic_cast<SimpleGainSpec*>(gain)) != nullptr) {
        int r = send_command((struct command) {0x03, htonl(1)});
        if (r < 0) {
            std::cerr << "setting gain mode failed" << std::endl;
            return 2;
        }

        r = send_command((struct command) {0x04, htonl(simple_gain->getValue() * 10)});
        if (r < 0) {
            std::cerr << "setting gain failed" << std::endl;
            return 3;
        }

        return 0;
    }

    std::cerr << "unsupported gain settings" << std::endl;
    return 100;
}

int GNURadioTcpConnector::set_ppm(double ppm) {
    return send_command((struct command) {0x05, htonl((int32_t) ppm)});
}

int GNURadioTcpConnector::set_direct_sampling(int direct_sampling) {
    int r = send_command((struct command) {0x09, htonl(direct_sampling)});
    if (r != 0) {
        std::cerr << "setting direct sampling failed with rc = " << r << std::endl;
        return r;
    }
    // switching direct sampling mode requires setting the frequency again
    r = set_center_frequency(get_center_frequency());
    if (r != 0) {
        std::cerr << "setting center frequency failed with rc = " << r << std::endl;
        return r;
    }
    if (direct_sampling == 0) {
        // gain is off when switching out of direct sampling, so reset it
        r = set_gain(get_gain());
        if (r != 0) {
            std::cerr << "setting gain failed with rc = " << r << std::endl;
            return r;
        }
    }
    return 0;
}

int GNURadioTcpConnector::set_bias_tee(bool bias_tee) {
    return send_command((struct command) {0x0e, htonl((unsigned int) bias_tee)});
}
