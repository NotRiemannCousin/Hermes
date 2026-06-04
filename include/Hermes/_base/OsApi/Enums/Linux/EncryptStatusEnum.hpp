#pragma once

namespace Hermes {
    // Mirror of the Windows EncryptStatusEnum, restricted to the codes the Hermes
    // state machines actually switch on. OpenSSL return codes are translated into
    // these values inside TlsSession.cpp so the state-machine logic stays the same
    // across platforms. Integer values are arbitrary — they only need to be unique.
    enum class EncryptStatusEnum {
        ErrOk                          = 0,

        ErrInsufficientMemory          = 1,
        ErrInvalidHandle               = 2,
        ErrInvalidToken                = 3,
        ErrLogonDenied                 = 4,
        ErrIncompleteMessage           = 5,
        ErrIncompleteCredentials       = 6,
        ErrUntrustedRoot               = 7,
        ErrNoCredentials               = 8,
        ErrNoAuthenticatingAuthority   = 9,
        ErrCertUnknown                 = 10,
        ErrCertExpired                 = 11,
        ErrInternalError               = 12,
        ErrUnknownCredentials          = 13,
        ErrUnsupportedFunction         = 14,
        ErrSecPkgNotFound              = 15,
        ErrEncryptFailure              = 16,
        ErrDecryptFailure              = 17,

        InfoContinueNeeded             = 100,
        InfoCompleteNeeded             = 101,
        InfoCompleteAndContinue        = 102,
        InfoRenegotiate                = 103,
        InfoContextExpired             = 104,
    };
}