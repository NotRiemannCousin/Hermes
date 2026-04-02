#pragma once
#include <winerror.h>

enum class EncryptStatusEnum {
    ErrOk                          = SEC_E_OK, // Everthing ok

    ErrInsufficientMemory          = SEC_E_INSUFFICIENT_MEMORY,           // Not enough memory is available to complete this request
    ErrInvalidHandle               = SEC_E_INVALID_HANDLE,                // The handle specified is invalid
    ErrUnsupportedFunction         = SEC_E_UNSUPPORTED_FUNCTION,          // The function requested is not supported
    ErrTargetUnknown               = SEC_E_TARGET_UNKNOWN,                // The specified target is unknown or unreachable
    ErrInternalError               = SEC_E_INTERNAL_ERROR,                // The Local Security Authority cannot be contacted
    ErrSecpkgNotFound              = SEC_E_SECPKG_NOT_FOUND,              // The requested security package does not exist
    ErrNotOwner                    = SEC_E_NOT_OWNER,                     // The caller is not the owner of the desired credentials
    ErrCannotInstall               = SEC_E_CANNOT_INSTALL,                // The security package failed to initialize, and cannot be installed
    ErrInvalidToken                = SEC_E_INVALID_TOKEN,                 // The token supplied to the function is invalid
    ErrCannotPack                  = SEC_E_CANNOT_PACK,                   // The security package is not able to marshall the logon buffer, so the logon attempt has failed
    ErrQopNotSupported             = SEC_E_QOP_NOT_SUPPORTED,             // The per-message Quality of Protection is not supported by the security package
    ErrNoImpersonation             = SEC_E_NO_IMPERSONATION,              // The security context does not allow impersonation of the client
    ErrLogonDenied                 = SEC_E_LOGON_DENIED,                  // The logon attempt failed
    ErrUnknownCredentials          = SEC_E_UNKNOWN_CREDENTIALS,           // The credentials supplied to the package were not recognized
    ErrNoCredentials               = SEC_E_NO_CREDENTIALS,                // No credentials are available in the security package
    ErrMessageAltered              = SEC_E_MESSAGE_ALTERED,               // The message or signature supplied for verification has been altered
    ErrOutOfSequence               = SEC_E_OUT_OF_SEQUENCE,               // The message supplied for verification is out of sequence
    ErrNoAuthenticatingAuthority   = SEC_E_NO_AUTHENTICATING_AUTHORITY,   // No authority could be contacted for authentication.
    ErrBadPkgid                    = SEC_E_BAD_PKGID,                     // The requested security package does not exist
    ErrContextExpired              = SEC_E_CONTEXT_EXPIRED,               // The context has expired and can no longer be used.
    ErrIncompleteMessage           = SEC_E_INCOMPLETE_MESSAGE,            // The supplied message is incomplete. The signature was not verified.
    ErrIncompleteCredentials       = SEC_E_INCOMPLETE_CREDENTIALS,        // The credentials supplied were not complete, and could not be verified. The context could not be initialized.
    ErrBufferTooSmall              = SEC_E_BUFFER_TOO_SMALL,              // The buffers supplied to a function was too small.
    ErrWrongPrincipal              = SEC_E_WRONG_PRINCIPAL,               // The target principal name is incorrect.
    ErrTimeSkew                    = SEC_E_TIME_SKEW,                     // The clocks on the client and server machines are skewed.
    ErrUntrustedRoot               = SEC_E_UNTRUSTED_ROOT,                // The certificate chain was issued by an authority that is not trusted.
    ErrIllegalMessage              = SEC_E_ILLEGAL_MESSAGE,               // The message received was unexpected or badly formatted.
    ErrCertUnknown                 = SEC_E_CERT_UNKNOWN,                  // An unknown error occurred while processing the certificate.
    ErrCertExpired                 = SEC_E_CERT_EXPIRED,                  // The received certificate has expired.
    ErrEncryptFailure              = SEC_E_ENCRYPT_FAILURE,               // The specified data could not be encrypted.
    ErrDecryptFailure              = SEC_E_DECRYPT_FAILURE,               //
    ErrAlgorithmMismatch           = SEC_E_ALGORITHM_MISMATCH,            // The client and server cannot communicate, because they do not possess a common algorithm.
    ErrSecurityQosFailed           = SEC_E_SECURITY_QOS_FAILED,           // The security context could not be established due to a failure in the requested quality of service (e.g. mutual authentication or delegation).
    ErrUnfinishedContextDeleted    = SEC_E_UNFINISHED_CONTEXT_DELETED,    // A security context was deleted before the context was completed. This is considered a logon failure.
    ErrNoTgtReply                  = SEC_E_NO_TGT_REPLY,                  // The client is trying to negotiate a context and the server requires user-to-user but didn't send a TGT reply.
    ErrNoIpAddresses               = SEC_E_NO_IP_ADDRESSES,               // Unable to accomplish the requested task because the local machine does not have any IP addresses.
    ErrWrongCredentialHandle       = SEC_E_WRONG_CREDENTIAL_HANDLE,       // The supplied credential handle does not match the credential associated with the security context.
    ErrCryptoSystemInvalid         = SEC_E_CRYPTO_SYSTEM_INVALID,         // The crypto system or checksum function is invalid because a required function is unavailable.
    ErrMaxReferralsExceeded        = SEC_E_MAX_REFERRALS_EXCEEDED,        // The number of maximum ticket referrals has been exceeded.
    ErrMustBeKdc                   = SEC_E_MUST_BE_KDC,                   // The local machine must be a Kerberos KDC (domain controller) and it is not.
    ErrStrongCryptoNotSupported    = SEC_E_STRONG_CRYPTO_NOT_SUPPORTED,   // The other end of the security negotiation is requires strong crypto but it is not supported on the local machine.
    ErrTooManyPrincipals           = SEC_E_TOO_MANY_PRINCIPALS,           // The KDC reply contained more than one principal name.
    ErrNoPaData                    = SEC_E_NO_PA_DATA,                    // Expected to find PA data for a hint of what etype to use, but it was not found.
    ErrPkinitNameMismatch          = SEC_E_PKINIT_NAME_MISMATCH,          // The client certificate does not contain a valid UPN, or does not match the client name in the logon request. Please contact your administrator.
    ErrSmartcardLogonRequired      = SEC_E_SMARTCARD_LOGON_REQUIRED,      // Smartcard logon is required and was not used.
    ErrShutdownInProgress          = SEC_E_SHUTDOWN_IN_PROGRESS,          // A system shutdown is in progress.
    ErrKdcInvalidRequest           = SEC_E_KDC_INVALID_REQUEST,           // An invalid request was sent to the KDC.
    ErrKdcUnableToRefer            = SEC_E_KDC_UNABLE_TO_REFER,           // The KDC was unable to generate a referral for the service requested.
    ErrKdcUnknownEtype             = SEC_E_KDC_UNKNOWN_ETYPE,             // The encryption type requested is not supported by the KDC.
    ErrUnsupportedPreauth          = SEC_E_UNSUPPORTED_PREAUTH,           // An unsupported preauthentication mechanism was presented to the Kerberos package.
    ErrDelegationRequired          = SEC_E_DELEGATION_REQUIRED,           // The requested operation cannot be completed. The computer must be trusted for delegation and the current user account must be configured to allow delegation.
    ErrBadBindings                 = SEC_E_BAD_BINDINGS,                  // Client's supplied SSPI channel bindings were incorrect.
    ErrMultipleAccounts            = SEC_E_MULTIPLE_ACCOUNTS,             // The received certificate was mapped to multiple accounts.
    ErrNoKerbKey                   = SEC_E_NO_KERB_KEY,                   //  SEC_E_NO_KERB_KEY
    ErrCertWrongUsage              = SEC_E_CERT_WRONG_USAGE,              // The certificate is not valid for the requested usage.
    ErrDowngradeDetected           = SEC_E_DOWNGRADE_DETECTED,            // The system cannot contact a domain controller to service the authentication request. Please try again later.
    ErrSmartcardCertRevoked        = SEC_E_SMARTCARD_CERT_REVOKED,        // The smartcard certificate used for authentication has been revoked. Please contact your system administrator. There may be additional information in the event log.
    ErrIssuingCaUntrusted          = SEC_E_ISSUING_CA_UNTRUSTED,          // An untrusted certificate authority was detected while processing the smartcard certificate used for authentication. Please contact your system administrator.
    ErrRevocationOfflineC          = SEC_E_REVOCATION_OFFLINE_C,          // The revocation status of the smartcard certificate used for authentication could not be determined. Please contact your system administrator.
    ErrPkinitClientFailure         = SEC_E_PKINIT_CLIENT_FAILURE,         // The smartcard certificate used for authentication was not trusted. Please contact your system administrator.
    ErrSmartcardCertExpired        = SEC_E_SMARTCARD_CERT_EXPIRED,        // The smartcard certificate used for authentication has expired. Please contact your system administrator.
    ErrNoS4UProtSupport            = SEC_E_NO_S4U_PROT_SUPPORT,           // The Kerberos subsystem encountered an error. A service for user protocol request was made against a domain controller which does not support service for user.
    ErrCrossrealmDelegationFailure = SEC_E_CROSSREALM_DELEGATION_FAILURE, // An attempt was made by this server to make a Kerberos constrained delegation request for a target outside of the server's realm. This is not supported, and indicates a misconfiguration on this server's allowed to delegate to list. Please contact your administrator.
    ErrRevocationOfflineKdc        = SEC_E_REVOCATION_OFFLINE_KDC,        // The revocation status of the domain controller certificate used for smartcard authentication could not be determined. There is additional information in the system event log. Please contact your system administrator.
    ErrIssuingCaUntrustedKdc       = SEC_E_ISSUING_CA_UNTRUSTED_KDC,      // An untrusted certificate authority was detected while processing the domain controller certificate used for authentication. There is additional information in the system event log. Please contact your system administrator.
    ErrKdcCertExpired              = SEC_E_KDC_CERT_EXPIRED,              // The domain controller certificate used for smartcard logon has expired. Please contact your system administrator with the contents of your system event log.
    ErrKdcCertRevoked              = SEC_E_KDC_CERT_REVOKED,              // The domain controller certificate used for smartcard logon has been revoked. Please contact your system administrator with the contents of your system event log.
    ErrInvalidParameter            = SEC_E_INVALID_PARAMETER,             // One or more of the parameters passed to the function was invalid.
    ErrDelegationPolicy            = SEC_E_DELEGATION_POLICY,             // Client policy does not allow credential delegation to target server.
    ErrPolicyNltmOnly              = SEC_E_POLICY_NLTM_ONLY,              // Client policy does not allow credential delegation to target server with NLTM only authentication.
    ErrNoContext                   = SEC_E_NO_CONTEXT,                    // The required security context does not exist.
    ErrPku2UCertFailure            = SEC_E_PKU2U_CERT_FAILURE,            // The PKU2U protocol encountered an error while attempting to utilize the associated certificates.
    ErrMutualAuthFailed            = SEC_E_MUTUAL_AUTH_FAILED,            // The identity of the server computer could not be verified.
    ErrOnlyHttpsAllowed            = SEC_E_ONLY_HTTPS_ALLOWED,            // Only https scheme is allowed.
    ErrApplicationProtocolMismatch = SEC_E_APPLICATION_PROTOCOL_MISMATCH, // No common application protocol exists between the client and the server. Application protocol negotiation failed.
    ErrInvalidUpnName              = SEC_E_INVALID_UPN_NAME,              // You can't sign in with a user ID in this format. Try using your email address instead.
    ErrExtBufferTooSmall           = SEC_E_EXT_BUFFER_TOO_SMALL,          // The buffer supplied by the SSPI caller to receive generic extensions is too small.
    ErrInsufficientBuffers         = SEC_E_INSUFFICIENT_BUFFERS,




    InfoContinueNeeded           = SEC_I_CONTINUE_NEEDED,                   // The function completed successfully, but must be called again to complete the context
    InfoCompleteNeeded           = SEC_I_COMPLETE_NEEDED,                   // The function completed successfully, but CompleteToken must be called
    InfoCompleteAndContinue      = SEC_I_COMPLETE_AND_CONTINUE,             // The function completed successfully, but both CompleteToken and this function must be called to complete the context
    InfoLocalLogon               = SEC_I_LOCAL_LOGON,                       // The logon was completed, but no network authority was available. The logon was made using locally known information
    InfoGenericExtensionReceived = SEC_I_GENERIC_EXTENSION_RECEIVED,        // Schannel has received a TLS extension the SSPI caller subscribed to.
    InfoContextExpired           = SEC_I_CONTEXT_EXPIRED,                   // The context has expired and can no longer be used.
    InfoIncompleteCredentials    = SEC_I_INCOMPLETE_CREDENTIALS,            // The credentials supplied were not complete, and could not be verified. Additional information can be returned from the context.
    InfoRenegotiate              = SEC_I_RENEGOTIATE,                       // The context data must be renegotiated with the peer.
    InfoNoLsaContext             = SEC_I_NO_LSA_CONTEXT,                    // There is no LSA mode context associated with this context.
    InfoSignatureNeeded          = SEC_I_SIGNATURE_NEEDED,                  // A signature operation must be performed before the user can authenticate.
    InfoNoRenegotiation          = SEC_I_NO_RENEGOTIATION,                  // The recipient rejected the renegotiation request.
    InfoMessageFragment          = SEC_I_MESSAGE_FRAGMENT,                  // The returned buffer is only a fragment of the message.  More fragments need to be returned.
    InfoContinueNeededMessageOk  = SEC_I_CONTINUE_NEEDED_MESSAGE_OK,        // The function completed successfully, but must be called again to complete the context.  Early start can be used.
    InfoAsyncCallPending         = SEC_I_ASYNC_CALL_PENDING,                // An asynchronous SSPI routine has been called and the work is pending completion.
};