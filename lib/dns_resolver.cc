#include <stdio.h>


#ifndef _WIN32
#include <netdb.h>
#include <cstring>
#define closesocket close
#else
#include <ws2tcpip.h>
#endif

namespace xameleon
{
	class dns_resolver
	{
		int iResult;
		struct addrinfo hints;
	public:
		dns_resolver()
		{
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
			hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
			hints.ai_flags = 0;
			hints.ai_protocol = 0;          /* Any protocol */
#ifdef _WIN32
			WSADATA wsaData;

			iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
			if (iResult != 0) {
				printf("WSAStartup failed: %d\n", iResult);
			}
#endif
		}

		~dns_resolver()
		{
#ifdef _WIN32
			WSACleanup();
#endif
		}

		bool get_address_by_name(const char* dns_name, struct sockaddr_in* paddress, int * addr_len)
		{
			bool result = false;
			int status;
			struct addrinfo* result_list;

			status = getaddrinfo(dns_name, "0", &hints, &result_list);
			if (status != 0) {
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
				return false;
			}

			// Пока берём последний адрес.
			for (struct addrinfo* rp = result_list; rp != NULL; rp = rp->ai_next) {
				*addr_len = rp->ai_addrlen;
				memcpy(paddress, rp->ai_addr, rp->ai_addrlen);
				result = true;
			}

			freeaddrinfo(result_list);           /* No longer needed */

			return result;
		}

	};

	static dns_resolver local_resolver;

	bool get_address_by_name(const char* dns_name, struct sockaddr_in* paddress, int* addr_len)
	{
		return local_resolver.get_address_by_name(dns_name, paddress, addr_len);
	}
}

