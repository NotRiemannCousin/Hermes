#pragma once
// This file it's a wrapper to hold the things from windows headers
// used to manage socket connections and other things. Almost every
// code here it's just the export of windows headers to C++20 modules.
// (and that it's why it's a mess

// I cant forward macros so some things are wrapped in enums, constexpr
// with "macro" prefix and similarities. There is also some utilities
// functions to make things easier.

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SECURITY_WIN32

#include <string_view>
#include <winsock2.h>
#include <schannel.h>
#include <ws2tcpip.h>
#include <sspi.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

template<class E> requires std::is_scoped_enum_v<E>
constexpr bool IsEnumFlag(){
    return std::string_view(__FUNCSIG__).ends_with("Flags>(void)");
}

template<class E>
concept EnumFlagConcept = requires { requires IsEnumFlag<E>(); };

#define FLAGS_OPERATIONS(TYPE) \
        static_assert(std::is_enum_v<TYPE>);                                                \
        constexpr TYPE operator|(TYPE f1, TYPE f2) {                                        \
            using type = std::underlying_type_t<decltype(f1)>;                              \
            return static_cast<TYPE>(type(f1) | type(f2));                                  \
        }                                                                                   \
                                                                                            \
        constexpr bool operator!=(auto a, TYPE b) noexcept {                                \
            return a != static_cast<std::underlying_type_t<decltype(b)>>(b);               \
        }                                                                                   \
        constexpr bool operator==(auto a, TYPE b) noexcept {                                \
            return a == static_cast<std::underlying_type_t<decltype(b)>>(b);               \
        }                                                                                   \

#define ENUM_OPERATIONS(TYPE)\
        constexpr bool operator!=(auto a, TYPE b) noexcept {                                \
            return a != static_cast<std::underlying_type_t<decltype(b)>>(b);               \
        }                                                                                   \
        constexpr bool operator==(auto a, TYPE b) noexcept {                                \
            return a == static_cast<std::underlying_type_t<decltype(b)>>(b);               \
        }                                                                                   \


// export {
    //! User-defined literal for uint8_t.
    //! Usage: '123_uc' creates an uint8_t with value 123.
    inline uint8_t operator""_uc(unsigned long long int n) { return static_cast<uint8_t>(n); }

    // ----------------------------------------------------------------------------------------------------
    //                             Socket Flags
    // ----------------------------------------------------------------------------------------------------

    // using RawSocket = std::unique_ptr<std::uint64_t>;

    enum class SocketTypeEnum : int {
        STREAM    = SOCK_STREAM,        /* stream socket */
        DGRAM     = SOCK_DGRAM,         /* datagram socket */
        RAW       = SOCK_RAW,           /* raw-protocol interface */
        RDM       = SOCK_RDM,           /* reliably-delivered message */
        SEQPACKET = SOCK_SEQPACKET,     /* sequenced packet stream */
    };

    
    enum class ProtocolBaseTypeEnum : int {
        UDP = IPPROTO_UDP,
        TCP = IPPROTO_TCP
    };
    
    enum class ProtocolTypeEnum : int {
        HTTP,
        HTTPS,
        WSS,
        FTP
    };

    constexpr ProtocolBaseTypeEnum getBaseProtocol(ProtocolTypeEnum p) {
        switch (p) {
            case ProtocolTypeEnum::HTTP:
            case ProtocolTypeEnum::HTTPS:
            case ProtocolTypeEnum::WSS:
            case ProtocolTypeEnum::FTP:
            default:
                return ProtocolBaseTypeEnum::TCP;
                // future support to UDP for HTTP3
                // ? When? I don't know
        }
    }

    constexpr size_t macroWINSOCK_VERSION = WINSOCK_VERSION;

    enum class AddressFamilyEnum {
        UNSPEC    = AF_UNSPEC,

        IPX       = AF_IPX,
        APPLETALK = AF_APPLETALK,
        NETBIOS   = AF_NETBIOS,

        // Actual useful stuff
        INET      = AF_INET,
        INET6     = AF_INET6,
        IRDA      = AF_IRDA,
        BTH       = AF_BTH,
    };


    enum class ConditionFunctionEnum : int {
        ACCEPT = CF_ACCEPT,
        REJECT = CF_REJECT,
        DEFER  = CF_DEFER
    };

    enum class SocketShutdownEnum : int { RECEIVE = SD_RECEIVE, SEND = SD_SEND, BOTH = SD_BOTH };

    constexpr size_t macroSG_UNCONSTRAINED_GROUP = SG_UNCONSTRAINED_GROUP;
    constexpr size_t macroSG_CONSTRAINED_GROUP = SG_CONSTRAINED_GROUP;

    constexpr size_t macroSO_TYPE          = SO_TYPE;
    constexpr size_t macroSOL_SOCKET       = SOL_SOCKET;
    constexpr size_t macroSO_PROTOCOL_INFO = SO_PROTOCOL_INFO;

    enum class NameInfoFlags {
        NOFQDN      = NI_NOFQDN,      /* Only return nodename portion for local hosts */
        NUMERICHOST = NI_NUMERICHOST, /* Return numeric form of the host's address */
        NAMEREQD    = NI_NAMEREQD,    /* Error if the host's name not in DNS */
        NUMERICSERV = NI_NUMERICSERV, /* Return numeric form of the service (port #) */
        DGRAM       = NI_DGRAM,       /* Service is a datagram service */

        MAXHOST = NI_MAXHOST, /* Max size of a fully-qualified domain name */
        MAXSERV = NI_MAXSERV, /* Max size of a service name */
    };

	FLAGS_OPERATIONS(NameInfoFlags)


    // ----------------------------------------------------------------------------------------------------
    //                             Error Codes
    // ----------------------------------------------------------------------------------------------------


    constexpr SOCKET macroINVALID_SOCKET = INVALID_SOCKET;
    constexpr size_t macroSOCKET_ERROR   = SOCKET_ERROR;



    // ----------------------------------------------------------------------------------------------------
    //                             SSL and TLS
    // ----------------------------------------------------------------------------------------------------

    enum class SCHCredEnum {
        V1              = SCH_CRED_V1,
        V2              = SCH_CRED_V2,
        V3              = SCH_CRED_V3,
        SCHANNEL        = SCHANNEL_CRED_VERSION,
        SCH_CREDENTIALS = SCH_CREDENTIALS_VERSION,
    };

    enum class CredentialFlags : unsigned long {
        INBOUND  = SECPKG_CRED_INBOUND,
        OUTBOUND = SECPKG_CRED_OUTBOUND,
        BOTH     = SECPKG_CRED_BOTH,
        DEFAULT  = SECPKG_CRED_DEFAULT,
        RESERVED = SECPKG_CRED_RESERVED
    };

	FLAGS_OPERATIONS(CredentialFlags)

    constexpr std::string_view macroUNISP_NAME{ UNISP_NAME };

    enum class SupportedProtocolsFlags : long long {
        PCT1_SERVER = SP_PROT_PCT1_SERVER,
        PCT1_CLIENT = SP_PROT_PCT1_CLIENT,
        PCT1        = SP_PROT_PCT1,

        SSL2_SERVER = SP_PROT_SSL2_SERVER,
        SSL2_CLIENT = SP_PROT_SSL2_CLIENT,
        SSL2        = SP_PROT_SSL2,

        SSL3_SERVER = SP_PROT_SSL3_SERVER,
        SSL3_CLIENT = SP_PROT_SSL3_CLIENT,
        SSL3        = SP_PROT_SSL3,

        TLS1_SERVER = SP_PROT_TLS1_SERVER,
        TLS1_CLIENT = SP_PROT_TLS1_CLIENT,
        TLS1        = SP_PROT_TLS1,

        SSL3TLS1_CLIENTS = SP_PROT_SSL3TLS1_CLIENTS,
        SSL3TLS1_SERVERS = SP_PROT_SSL3TLS1_SERVERS,
        SSL3TLS1         = SP_PROT_SSL3TLS1,

        UNI_SERVER = SP_PROT_UNI_SERVER,
        UNI_CLIENT = SP_PROT_UNI_CLIENT,
        UNI        = SP_PROT_UNI,

        ALL     = SP_PROT_ALL,
        NONE    = SP_PROT_NONE,
        CLIENTS = SP_PROT_CLIENTS,
        SERVERS = SP_PROT_SERVERS,


        TLS1_0_SERVER = SP_PROT_TLS1_0_SERVER,
        TLS1_0_CLIENT = SP_PROT_TLS1_0_CLIENT,
        TLS1_0        = SP_PROT_TLS1_0,

        TLS1_1_SERVER = SP_PROT_TLS1_1_SERVER,
        TLS1_1_CLIENT = SP_PROT_TLS1_1_CLIENT,
        TLS1_1        = SP_PROT_TLS1_1,

        TLS1_2_SERVER = SP_PROT_TLS1_2_SERVER,
        TLS1_2_CLIENT = SP_PROT_TLS1_2_CLIENT,
        TLS1_2        = SP_PROT_TLS1_2,

        TLS1_3_SERVER = SP_PROT_TLS1_3_SERVER,
        TLS1_3_CLIENT = SP_PROT_TLS1_3_CLIENT,
        TLS1_3        = SP_PROT_TLS1_3,

        DTLS_SERVER = SP_PROT_DTLS_SERVER,
        DTLS_CLIENT = SP_PROT_DTLS_CLIENT,
        DTLS        = SP_PROT_DTLS,

        DTLS1_0_SERVER = SP_PROT_DTLS1_0_SERVER,
        DTLS1_0_CLIENT = SP_PROT_DTLS1_0_CLIENT,
        DTLS1_0        = SP_PROT_DTLS1_0,

        DTLS1_2_SERVER = SP_PROT_DTLS1_2_SERVER,
        DTLS1_2_CLIENT = SP_PROT_DTLS1_2_CLIENT,
        DTLS1_2        = SP_PROT_DTLS1_2,

        DTLS1_X_SERVER = SP_PROT_DTLS1_X_SERVER,

        DTLS1_X_CLIENT = SP_PROT_DTLS1_X_CLIENT,

        DTLS1_X = SP_PROT_DTLS1_X,

        TLS1_1PLUS_SERVER = SP_PROT_TLS1_1PLUS_SERVER,
        TLS1_1PLUS_CLIENT = SP_PROT_TLS1_1PLUS_CLIENT,

        TLS1_1PLUS = SP_PROT_TLS1_1PLUS,

        TLS1_3PLUS_SERVER = SP_PROT_TLS1_3PLUS_SERVER,
        TLS1_3PLUS_CLIENT = SP_PROT_TLS1_3PLUS_CLIENT,
        TLS1_3PLUS        = SP_PROT_TLS1_3PLUS,

        TLS1_X_SERVER = SP_PROT_TLS1_X_SERVER,
        TLS1_X_CLIENT = SP_PROT_TLS1_X_CLIENT,
        TLS1_X        = SP_PROT_TLS1_X,

        SSL3TLS1_X_CLIENTS = SP_PROT_SSL3TLS1_X_CLIENTS,
        SSL3TLS1_X_SERVERS = SP_PROT_SSL3TLS1_X_SERVERS,
        SSL3TLS1_X         = SP_PROT_SSL3TLS1_X,

        X_CLIENTS = SP_PROT_X_CLIENTS,
        X_SERVERS = SP_PROT_X_SERVERS
    };
    FLAGS_OPERATIONS(SupportedProtocolsFlags)

    enum class InitializeSecurityContextFlags : long long {
        DELEGATE               = ISC_REQ_DELEGATE,
        MUTUAL_AUTH            = ISC_REQ_MUTUAL_AUTH,
        REPLAY_DETECT          = ISC_REQ_REPLAY_DETECT,
        SEQUENCE_DETECT        = ISC_REQ_SEQUENCE_DETECT,
        CONFIDENTIALITY        = ISC_REQ_CONFIDENTIALITY,
        USE_SESSION_KEY        = ISC_REQ_USE_SESSION_KEY,
        PROMPT_FOR_CREDS       = ISC_REQ_PROMPT_FOR_CREDS,
        USE_SUPPLIED_CREDS     = ISC_REQ_USE_SUPPLIED_CREDS,
        ALLOCATE_MEMORY        = ISC_REQ_ALLOCATE_MEMORY,
        USE_DCE_STYLE          = ISC_REQ_USE_DCE_STYLE,
        DATAGRAM               = ISC_REQ_DATAGRAM,
        CONNECTION             = ISC_REQ_CONNECTION,
        CALL_LEVEL             = ISC_REQ_CALL_LEVEL,
        FRAGMENT_SUPPLIED      = ISC_REQ_FRAGMENT_SUPPLIED,
        EXTENDED_ERROR         = ISC_REQ_EXTENDED_ERROR,
        STREAM                 = ISC_REQ_STREAM,
        INTEGRITY              = ISC_REQ_INTEGRITY,
        IDENTIFY               = ISC_REQ_IDENTIFY,
        NULL_SESSION           = ISC_REQ_NULL_SESSION,
        MANUAL_CRED_VALIDATION = ISC_REQ_MANUAL_CRED_VALIDATION,
        RESERVED1              = ISC_REQ_RESERVED1,
        FRAGMENT_TO_FIT        = ISC_REQ_FRAGMENT_TO_FIT,
        // This exists only in Windows Vista and greater
        FORWARD_CREDENTIALS    = ISC_REQ_FORWARD_CREDENTIALS,
        NO_INTEGRITY           = ISC_REQ_NO_INTEGRITY, // honored only by SPNEGO
        USE_HTTP_STYLE         = ISC_REQ_USE_HTTP_STYLE,
        UNVERIFIED_TARGET_NAME = ISC_REQ_UNVERIFIED_TARGET_NAME,
        CONFIDENTIALITY_ONLY   = ISC_REQ_CONFIDENTIALITY_ONLY, // honored by SPNEGO/Kerberos
        MESSAGES               = ISC_REQ_MESSAGES, // Disables the TLS 1.3+ record layer and causes the security context to
                                         // consume and produce cleartext TLS messages, rather than records.
        // Request that schannel perform server cert chain validation without failing the handshake on errors
        // (deferred), same as SCH_CRED_DEFERRED_CRED_VALIDATION except applies to context not credential handle.
        DEFERRED_CRED_VALIDATION = ISC_REQ_DEFERRED_CRED_VALIDATION,
        // Prevents the client sending the post_handshake_auth extension in the TLS 1.3 Client Hello.
        NO_POST_HANDSHAKE_AUTH = ISC_REQ_NO_POST_HANDSHAKE_AUTH,

    };

	FLAGS_OPERATIONS(InitializeSecurityContextFlags)

    enum class InitializeSecurityContextReturnFlags : long long {
        RET_DELEGATE               = ISC_RET_DELEGATE,
        RET_MUTUAL_AUTH            = ISC_RET_MUTUAL_AUTH,
        RET_REPLAY_DETECT          = ISC_RET_REPLAY_DETECT,
        RET_SEQUENCE_DETECT        = ISC_RET_SEQUENCE_DETECT,
        RET_CONFIDENTIALITY        = ISC_RET_CONFIDENTIALITY,
        RET_USE_SESSION_KEY        = ISC_RET_USE_SESSION_KEY,
        RET_USED_COLLECTED_CREDS   = ISC_RET_USED_COLLECTED_CREDS,
        RET_USED_SUPPLIED_CREDS    = ISC_RET_USED_SUPPLIED_CREDS,
        RET_ALLOCATED_MEMORY       = ISC_RET_ALLOCATED_MEMORY,
        RET_USED_DCE_STYLE         = ISC_RET_USED_DCE_STYLE,
        RET_DATAGRAM               = ISC_RET_DATAGRAM,
        RET_CONNECTION             = ISC_RET_CONNECTION,
        RET_INTERMEDIATE_RETURN    = ISC_RET_INTERMEDIATE_RETURN,
        RET_CALL_LEVEL             = ISC_RET_CALL_LEVEL,
        RET_EXTENDED_ERROR         = ISC_RET_EXTENDED_ERROR,
        RET_STREAM                 = ISC_RET_STREAM,
        RET_INTEGRITY              = ISC_RET_INTEGRITY,
        RET_IDENTIFY               = ISC_RET_IDENTIFY,
        RET_NULL_SESSION           = ISC_RET_NULL_SESSION,
        RET_MANUAL_CRED_VALIDATION = ISC_RET_MANUAL_CRED_VALIDATION,
        RET_RESERVED1              = ISC_RET_RESERVED1,
        RET_FRAGMENT_ONLY          = ISC_RET_FRAGMENT_ONLY,
        // This exists only in Windows Vista and greater
        RET_FORWARD_CREDENTIALS = ISC_RET_FORWARD_CREDENTIALS,

        RET_USED_HTTP_STYLE          = ISC_RET_USED_HTTP_STYLE,
        RET_NO_ADDITIONAL_TOKEN      = ISC_RET_NO_ADDITIONAL_TOKEN,
        RET_REAUTHENTICATION         = ISC_RET_REAUTHENTICATION,
        RET_CONFIDENTIALITY_ONLY     = ISC_RET_CONFIDENTIALITY_ONLY,
        RET_MESSAGES                 = ISC_RET_MESSAGES,
        RET_DEFERRED_CRED_VALIDATION = ISC_RET_DEFERRED_CRED_VALIDATION,
        RET_NO_POST_HANDSHAKE_AUTH   = ISC_RET_NO_POST_HANDSHAKE_AUTH,
    };

	FLAGS_OPERATIONS(InitializeSecurityContextReturnFlags)

    enum class SchannelCredFlags {
        NO_DEFAULT_CREDS       = SCH_CRED_NO_DEFAULT_CREDS,
        MANUAL_CRED_VALIDATION = SCH_CRED_MANUAL_CRED_VALIDATION
    };

	FLAGS_OPERATIONS(SchannelCredFlags)


    enum class SecurityBufferEnum : long long {
        EMPTY                    = SECBUFFER_EMPTY,            // Undefined, replaced by provider
        DATA                     = SECBUFFER_DATA,             // Packet data
        TOKEN                    = SECBUFFER_TOKEN,            // Security token
        PKG_PARAMS               = SECBUFFER_PKG_PARAMS,       // Package specific parameters
        MISSING                  = SECBUFFER_MISSING,          // Missing Data indicator
        EXTRA                    = SECBUFFER_EXTRA,            // Extra data
        STREAM_TRAILER           = SECBUFFER_STREAM_TRAILER,   // Security Trailer
        STREAM_HEADER            = SECBUFFER_STREAM_HEADER,    // Security Header
        NEGOTIATION_INFO         = SECBUFFER_NEGOTIATION_INFO, // Hints from the negotiation pkg
        PADDING                  = SECBUFFER_PADDING,          // non-data padding
        STREAM                   = SECBUFFER_STREAM,           // whole encrypted message
        MECHLIST                 = SECBUFFER_MECHLIST,
        MECHLIST_SIGNATURE       = SECBUFFER_MECHLIST_SIGNATURE,
        TARGET                   = SECBUFFER_TARGET, // obsolete
        CHANNEL_BINDINGS         = SECBUFFER_CHANNEL_BINDINGS,
        CHANGE_PASS_RESPONSE     = SECBUFFER_CHANGE_PASS_RESPONSE,
        TARGET_HOST              = SECBUFFER_TARGET_HOST,
        ALERT                    = SECBUFFER_ALERT,
        APPLICATION_PROTOCOLS    = SECBUFFER_APPLICATION_PROTOCOLS,    // Lists of application protocol IDs, one per negotiation extension
        SRTP_PROTECTION_PROFILES = SECBUFFER_SRTP_PROTECTION_PROFILES, // List of SRTP protection profiles, in
        // descending order of preference
        SRTP_MASTER_KEY_IDENTIFIER      = SECBUFFER_SRTP_MASTER_KEY_IDENTIFIER,      // SRTP master key identifier
        TOKEN_BINDING                   = SECBUFFER_TOKEN_BINDING,                   // Supported Token Binding protocol version and key parameters
        PRESHARED_KEY                   = SECBUFFER_PRESHARED_KEY,                   // Preshared key
        PRESHARED_KEY_IDENTITY          = SECBUFFER_PRESHARED_KEY_IDENTITY,          // Preshared key identity
        DTLS_MTU                        = SECBUFFER_DTLS_MTU,                        // DTLS path MTU setting
        SEND_GENERIC_TLS_EXTENSION      = SECBUFFER_SEND_GENERIC_TLS_EXTENSION,      // Buffer for sending generic TLS extensions.
        SUBSCRIBE_GENERIC_TLS_EXTENSION = SECBUFFER_SUBSCRIBE_GENERIC_TLS_EXTENSION, // Buffer for subscribing to generic TLS extensions.
        FLAGS                           = SECBUFFER_FLAGS,                           // ISC/ASC REQ Flags
        TRAFFIC_SECRETS                 = SECBUFFER_TRAFFIC_SECRETS,                 // Message sequence lengths and corresponding traffic secrets.
        CERTIFICATE_REQUEST_CONTEXT     = SECBUFFER_CERTIFICATE_REQUEST_CONTEXT,     // TLS 1.3 certificate request context.

        ATTRMASK               = SECBUFFER_ATTRMASK,
        READONLY               = SECBUFFER_READONLY,              // Buffer is read-only, no checksum
        READONLY_WITH_CHECKSUM = SECBUFFER_READONLY_WITH_CHECKSUM,// Buffer is read-only, and checksummed
        RESERVED               = SECBUFFER_RESERVED,              // Flags reserved to security system
    };

    ENUM_OPERATIONS(SecurityBufferEnum)

    using ::CredHandle;
    using ::CtxtHandle;
    using ::SCHANNEL_CRED;
    using ::TimeStamp;


    using ::SecBuffer;
    using ::SecBufferDesc;

    using ::SECURITY_STATUS;

    using ::AcquireCredentialsHandleA;
    using ::AcquireCredentialsHandleW;
    using ::FreeCredentialsHandle;


    using ::InitializeSecurityContextA;
    using ::InitializeSecurityContextW;
    using ::CompleteAuthToken;


    using ::EncryptMessage;
    using ::DecryptMessage;

    using ::QueryContextAttributesA;
    using ::QueryContextAttributesW;
    using ::SecPkgContext_StreamSizes;

    //----------------------------------------------------------------------------------------------------
    // Enum to integral and utils
    //----------------------------------------------------------------------------------------------------


    long long _tll(auto t) {
        return static_cast<long long>(t);
    }

    unsigned long long _tull(auto t) {
        return static_cast<unsigned long long>(t);
    }

    long _tl(auto t) {
        return static_cast<long>(t);
    }

    unsigned long _tul(auto t) {
        return static_cast<unsigned long>(t);
    }


    unsigned long _tus(auto t) {
        return static_cast<unsigned short>(t);
    }

    // EnumFlagConcept auto operator|(EnumFlagConcept auto f1, EnumFlagConcept auto f2) {
    //     using type = std::underlying_type_t<decltype(f1)>;
    //     return type(f1) | type(f2);
    // }
    //
    // EnumFlagConcept auto operator&(EnumFlagConcept auto f1, EnumFlagConcept auto f2) {
    //     using type = std::underlying_type_t<decltype(f1)>;
    //     return type(f1) & type(f2);
    // }
    //
    // EnumFlagConcept auto operator^(EnumFlagConcept auto f1, EnumFlagConcept auto f2) {
    //     using type = std::underlying_type_t<decltype(f1)>;
    //     return type(f1) ^ type(f2);
    // }

    // operator int(EnumFlagConcept auto e) {
    //     return static_cast<int>(e);
    // }
    //
    // operator unsigned long(EnumFlagConcept auto e) {
    //     return static_cast<unsigned long>(e);
    // }



    constexpr unsigned long macroSECBUFFER_VERSION = SECBUFFER_VERSION;
    constexpr unsigned long macroSECPKG_ATTR_STREAM_SIZES    = SECPKG_ATTR_STREAM_SIZES;

    enum class EncryptStatusEnum {
        ERR_OK                    = SEC_E_OK,
        ERR_BUFFER_TOO_SMALL      = SEC_E_BUFFER_TOO_SMALL,      //The message buffer is too small. Used with the Digest SSP.
        ERR_CRYPTO_SYSTEM_INVALID = SEC_E_CRYPTO_SYSTEM_INVALID, //The cipher chosen for the security context is not supported. Used with the Digest SSP.
        ERR_INCOMPLETE_MESSAGE    = SEC_E_INCOMPLETE_MESSAGE,    //The data in the input buffer is incomplete. The application needs to read more data from the server and call DecryptMessage (Digest) again.
        ERR_INVALID_HANDLE        = SEC_E_INVALID_HANDLE,        //A context handle that is not valid was specified in the phContext parameter. Used with the Digest SSP.
        ERR_MESSAGE_ALTERED       = SEC_E_MESSAGE_ALTERED,       //The message has been altered. Used with the Digest SSP.
        ERR_OUT_OF_SEQUENCE       = SEC_E_OUT_OF_SEQUENCE,       //The message was not received in the correct sequence.
        ERR_QOP_NOT_SUPPORTED     = SEC_E_QOP_NOT_SUPPORTED,     //Neither confidentiality nor integrity are supported by the security context. Used with the Digest SSP.


        INFO_CONTINUE_NEEDED            = SEC_I_CONTINUE_NEEDED,                   // The function completed successfully, but must be called again to complete the context
        INFO_COMPLETE_NEEDED            = SEC_I_COMPLETE_NEEDED,                   // The function completed successfully, but CompleteToken must be called
        INFO_COMPLETE_AND_CONTINUE      = SEC_I_COMPLETE_AND_CONTINUE,             // The function completed successfully, but both CompleteToken and this function must be called to complete the context
        INFO_LOCAL_LOGON                = SEC_I_LOCAL_LOGON,                       // The logon was completed, but no network authority was available. The logon was made using locally known information
        INFO_GENERIC_EXTENSION_RECEIVED = SEC_I_GENERIC_EXTENSION_RECEIVED,        // Schannel has received a TLS extension the SSPI caller subscribed to.
        INFO_CONTEXT_EXPIRED            = SEC_I_CONTEXT_EXPIRED,                   // The context has expired and can no longer be used.
        INFO_INCOMPLETE_CREDENTIALS     = SEC_I_INCOMPLETE_CREDENTIALS,            // The credentials supplied were not complete, and could not be verified. Additional information can be returned from the context.
        INFO_RENEGOTIATE                = SEC_I_RENEGOTIATE,                       // The context data must be renegotiated with the peer.
        INFO_NO_LSA_CONTEXT             = SEC_I_NO_LSA_CONTEXT,                    // There is no LSA mode context associated with this context.
        INFO_SIGNATURE_NEEDED           = SEC_I_SIGNATURE_NEEDED,                  // A signature operation must be performed before the user can authenticate.
        INFO_NO_RENEGOTIATION           = SEC_I_NO_RENEGOTIATION,                  // The recipient rejected the renegotiation request.
        INFO_MESSAGE_FRAGMENT           = SEC_I_MESSAGE_FRAGMENT,                  // The returned buffer is only a fragment of the message.  More fragments need to be returned.
        INFO_CONTINUE_NEEDED_MESSAGE_OK = SEC_I_CONTINUE_NEEDED_MESSAGE_OK,        // The function completed successfully, but must be called again to complete the context.  Early start can be used.
        INFO_ASYNC_CALL_PENDING         = SEC_I_ASYNC_CALL_PENDING,                // An asynchronous SSPI routine has been called and the work is pending completion.
    };
    ENUM_OPERATIONS(EncryptStatusEnum)

    constexpr bool macroSUCCEEDED(auto &&d) { return SUCCEEDED(d); }

    constexpr bool macroFAILED(auto &&d) { return FAILED(d); }
// };

#undef FLAGS_OPERATIONS
#undef ENUM_OPERATIONS