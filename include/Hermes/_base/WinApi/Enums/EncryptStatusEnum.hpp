#pragma once
#include <winerror.h>

enum class EncryptStatusEnum {
    ERR_OK                            = SEC_E_OK, // Everthing ok

    ERR_INSUFFICIENT_MEMORY           = SEC_E_INSUFFICIENT_MEMORY,           // Not enough memory is available to complete this request
    ERR_INVALID_HANDLE                = SEC_E_INVALID_HANDLE,                // The handle specified is invalid
    ERR_UNSUPPORTED_FUNCTION          = SEC_E_UNSUPPORTED_FUNCTION,          // The function requested is not supported
    ERR_TARGET_UNKNOWN                = SEC_E_TARGET_UNKNOWN,                // The specified target is unknown or unreachable
    ERR_INTERNAL_ERROR                = SEC_E_INTERNAL_ERROR,                // The Local Security Authority cannot be contacted
    ERR_SECPKG_NOT_FOUND              = SEC_E_SECPKG_NOT_FOUND,              // The requested security package does not exist
    ERR_NOT_OWNER                     = SEC_E_NOT_OWNER,                     // The caller is not the owner of the desired credentials
    ERR_CANNOT_INSTALL                = SEC_E_CANNOT_INSTALL,                // The security package failed to initialize, and cannot be installed
    ERR_INVALID_TOKEN                 = SEC_E_INVALID_TOKEN,                 // The token supplied to the function is invalid
    ERR_CANNOT_PACK                   = SEC_E_CANNOT_PACK,                   // The security package is not able to marshall the logon buffer, so the logon attempt has failed
    ERR_QOP_NOT_SUPPORTED             = SEC_E_QOP_NOT_SUPPORTED,             // The per-message Quality of Protection is not supported by the security package
    ERR_NO_IMPERSONATION              = SEC_E_NO_IMPERSONATION,              // The security context does not allow impersonation of the client
    ERR_LOGON_DENIED                  = SEC_E_LOGON_DENIED,                  // The logon attempt failed
    ERR_UNKNOWN_CREDENTIALS           = SEC_E_UNKNOWN_CREDENTIALS,           // The credentials supplied to the package were not recognized
    ERR_NO_CREDENTIALS                = SEC_E_NO_CREDENTIALS,                // No credentials are available in the security package
    ERR_MESSAGE_ALTERED               = SEC_E_MESSAGE_ALTERED,               // The message or signature supplied for verification has been altered
    ERR_OUT_OF_SEQUENCE               = SEC_E_OUT_OF_SEQUENCE,               // The message supplied for verification is out of sequence
    ERR_NO_AUTHENTICATING_AUTHORITY   = SEC_E_NO_AUTHENTICATING_AUTHORITY,   // No authority could be contacted for authentication.
    ERR_BAD_PKGID                     = SEC_E_BAD_PKGID,                     // The requested security package does not exist
    ERR_CONTEXT_EXPIRED               = SEC_E_CONTEXT_EXPIRED,               // The context has expired and can no longer be used.
    ERR_INCOMPLETE_MESSAGE            = SEC_E_INCOMPLETE_MESSAGE,            // The supplied message is incomplete. The signature was not verified.
    ERR_INCOMPLETE_CREDENTIALS        = SEC_E_INCOMPLETE_CREDENTIALS,        // The credentials supplied were not complete, and could not be verified. The context could not be initialized.
    ERR_BUFFER_TOO_SMALL              = SEC_E_BUFFER_TOO_SMALL,              // The buffers supplied to a function was too small.
    ERR_WRONG_PRINCIPAL               = SEC_E_WRONG_PRINCIPAL,               // The target principal name is incorrect.
    ERR_TIME_SKEW                     = SEC_E_TIME_SKEW,                     // The clocks on the client and server machines are skewed.
    ERR_UNTRUSTED_ROOT                = SEC_E_UNTRUSTED_ROOT,                // The certificate chain was issued by an authority that is not trusted.
    ERR_ILLEGAL_MESSAGE               = SEC_E_ILLEGAL_MESSAGE,               // The message received was unexpected or badly formatted.
    ERR_CERT_UNKNOWN                  = SEC_E_CERT_UNKNOWN,                  // An unknown error occurred while processing the certificate.
    ERR_CERT_EXPIRED                  = SEC_E_CERT_EXPIRED,                  // The received certificate has expired.
    ERR_ENCRYPT_FAILURE               = SEC_E_ENCRYPT_FAILURE,               // The specified data could not be encrypted.
    ERR_DECRYPT_FAILURE               = SEC_E_DECRYPT_FAILURE,               //
    ERR_ALGORITHM_MISMATCH            = SEC_E_ALGORITHM_MISMATCH,            // The client and server cannot communicate, because they do not possess a common algorithm.
    ERR_SECURITY_QOS_FAILED           = SEC_E_SECURITY_QOS_FAILED,           // The security context could not be established due to a failure in the requested quality of service (e.g. mutual authentication or delegation).
    ERR_UNFINISHED_CONTEXT_DELETED    = SEC_E_UNFINISHED_CONTEXT_DELETED,    // A security context was deleted before the context was completed. This is considered a logon failure.
    ERR_NO_TGT_REPLY                  = SEC_E_NO_TGT_REPLY,                  // The client is trying to negotiate a context and the server requires user-to-user but didn't send a TGT reply.
    ERR_NO_IP_ADDRESSES               = SEC_E_NO_IP_ADDRESSES,               // Unable to accomplish the requested task because the local machine does not have any IP addresses.
    ERR_WRONG_CREDENTIAL_HANDLE       = SEC_E_WRONG_CREDENTIAL_HANDLE,       // The supplied credential handle does not match the credential associated with the security context.
    ERR_CRYPTO_SYSTEM_INVALID         = SEC_E_CRYPTO_SYSTEM_INVALID,         // The crypto system or checksum function is invalid because a required function is unavailable.
    ERR_MAX_REFERRALS_EXCEEDED        = SEC_E_MAX_REFERRALS_EXCEEDED,        // The number of maximum ticket referrals has been exceeded.
    ERR_MUST_BE_KDC                   = SEC_E_MUST_BE_KDC,                   // The local machine must be a Kerberos KDC (domain controller) and it is not.
    ERR_STRONG_CRYPTO_NOT_SUPPORTED   = SEC_E_STRONG_CRYPTO_NOT_SUPPORTED,   // The other end of the security negotiation is requires strong crypto but it is not supported on the local machine.
    ERR_TOO_MANY_PRINCIPALS           = SEC_E_TOO_MANY_PRINCIPALS,           // The KDC reply contained more than one principal name.
    ERR_NO_PA_DATA                    = SEC_E_NO_PA_DATA,                    // Expected to find PA data for a hint of what etype to use, but it was not found.
    ERR_PKINIT_NAME_MISMATCH          = SEC_E_PKINIT_NAME_MISMATCH,          // The client certificate does not contain a valid UPN, or does not match the client name in the logon request. Please contact your administrator.
    ERR_SMARTCARD_LOGON_REQUIRED      = SEC_E_SMARTCARD_LOGON_REQUIRED,      // Smartcard logon is required and was not used.
    ERR_SHUTDOWN_IN_PROGRESS          = SEC_E_SHUTDOWN_IN_PROGRESS,          // A system shutdown is in progress.
    ERR_KDC_INVALID_REQUEST           = SEC_E_KDC_INVALID_REQUEST,           // An invalid request was sent to the KDC.
    ERR_KDC_UNABLE_TO_REFER           = SEC_E_KDC_UNABLE_TO_REFER,           // The KDC was unable to generate a referral for the service requested.
    ERR_KDC_UNKNOWN_ETYPE             = SEC_E_KDC_UNKNOWN_ETYPE,             // The encryption type requested is not supported by the KDC.
    ERR_UNSUPPORTED_PREAUTH           = SEC_E_UNSUPPORTED_PREAUTH,           // An unsupported preauthentication mechanism was presented to the Kerberos package.
    ERR_DELEGATION_REQUIRED           = SEC_E_DELEGATION_REQUIRED,           // The requested operation cannot be completed. The computer must be trusted for delegation and the current user account must be configured to allow delegation.
    ERR_BAD_BINDINGS                  = SEC_E_BAD_BINDINGS,                  // Client's supplied SSPI channel bindings were incorrect.
    ERR_MULTIPLE_ACCOUNTS             = SEC_E_MULTIPLE_ACCOUNTS,             // The received certificate was mapped to multiple accounts.
    ERR_NO_KERB_KEY                   = SEC_E_NO_KERB_KEY,                   //  SEC_E_NO_KERB_KEY
    ERR_CERT_WRONG_USAGE              = SEC_E_CERT_WRONG_USAGE,              // The certificate is not valid for the requested usage.
    ERR_DOWNGRADE_DETECTED            = SEC_E_DOWNGRADE_DETECTED,            // The system cannot contact a domain controller to service the authentication request. Please try again later.
    ERR_SMARTCARD_CERT_REVOKED        = SEC_E_SMARTCARD_CERT_REVOKED,        // The smartcard certificate used for authentication has been revoked. Please contact your system administrator. There may be additional information in the event log.
    ERR_ISSUING_CA_UNTRUSTED          = SEC_E_ISSUING_CA_UNTRUSTED,          // An untrusted certificate authority was detected while processing the smartcard certificate used for authentication. Please contact your system administrator.
    ERR_REVOCATION_OFFLINE_C          = SEC_E_REVOCATION_OFFLINE_C,          // The revocation status of the smartcard certificate used for authentication could not be determined. Please contact your system administrator.
    ERR_PKINIT_CLIENT_FAILURE         = SEC_E_PKINIT_CLIENT_FAILURE,         // The smartcard certificate used for authentication was not trusted. Please contact your system administrator.
    ERR_SMARTCARD_CERT_EXPIRED        = SEC_E_SMARTCARD_CERT_EXPIRED,        // The smartcard certificate used for authentication has expired. Please contact your system administrator.
    ERR_NO_S4U_PROT_SUPPORT           = SEC_E_NO_S4U_PROT_SUPPORT,           // The Kerberos subsystem encountered an error. A service for user protocol request was made against a domain controller which does not support service for user.
    ERR_CROSSREALM_DELEGATION_FAILURE = SEC_E_CROSSREALM_DELEGATION_FAILURE, // An attempt was made by this server to make a Kerberos constrained delegation request for a target outside of the server's realm. This is not supported, and indicates a misconfiguration on this server's allowed to delegate to list. Please contact your administrator.
    ERR_REVOCATION_OFFLINE_KDC        = SEC_E_REVOCATION_OFFLINE_KDC,        // The revocation status of the domain controller certificate used for smartcard authentication could not be determined. There is additional information in the system event log. Please contact your system administrator.
    ERR_ISSUING_CA_UNTRUSTED_KDC      = SEC_E_ISSUING_CA_UNTRUSTED_KDC,      // An untrusted certificate authority was detected while processing the domain controller certificate used for authentication. There is additional information in the system event log. Please contact your system administrator.
    ERR_KDC_CERT_EXPIRED              = SEC_E_KDC_CERT_EXPIRED,              // The domain controller certificate used for smartcard logon has expired. Please contact your system administrator with the contents of your system event log.
    ERR_KDC_CERT_REVOKED              = SEC_E_KDC_CERT_REVOKED,              // The domain controller certificate used for smartcard logon has been revoked. Please contact your system administrator with the contents of your system event log.
    ERR_INVALID_PARAMETER             = SEC_E_INVALID_PARAMETER,             // One or more of the parameters passed to the function was invalid.
    ERR_DELEGATION_POLICY             = SEC_E_DELEGATION_POLICY,             // Client policy does not allow credential delegation to target server.
    ERR_POLICY_NLTM_ONLY              = SEC_E_POLICY_NLTM_ONLY,              // Client policy does not allow credential delegation to target server with NLTM only authentication.
    ERR_NO_CONTEXT                    = SEC_E_NO_CONTEXT,                    // The required security context does not exist.
    ERR_PKU2U_CERT_FAILURE            = SEC_E_PKU2U_CERT_FAILURE,            // The PKU2U protocol encountered an error while attempting to utilize the associated certificates.
    ERR_MUTUAL_AUTH_FAILED            = SEC_E_MUTUAL_AUTH_FAILED,            // The identity of the server computer could not be verified.
    ERR_ONLY_HTTPS_ALLOWED            = SEC_E_ONLY_HTTPS_ALLOWED,            // Only https scheme is allowed.
    ERR_APPLICATION_PROTOCOL_MISMATCH = SEC_E_APPLICATION_PROTOCOL_MISMATCH, // No common application protocol exists between the client and the server. Application protocol negotiation failed.
    ERR_INVALID_UPN_NAME              = SEC_E_INVALID_UPN_NAME,              // You can't sign in with a user ID in this format. Try using your email address instead.
    ERR_EXT_BUFFER_TOO_SMALL          = SEC_E_EXT_BUFFER_TOO_SMALL,          // The buffer supplied by the SSPI caller to receive generic extensions is too small.
    ERR_INSUFFICIENT_BUFFERS          = SEC_E_INSUFFICIENT_BUFFERS,




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