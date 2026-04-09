#pragma once
#include <string>

namespace sf {

// Send a verification code to the given email address.
// Returns true if the email was dispatched (async, may still fail).
bool SendVerifyCode(const std::string& toEmail);

// Check if the user-entered code matches the one sent.
bool CheckVerifyCode(const std::string& email, const std::string& code);

// Returns true if a code was recently sent and is still valid (not expired).
bool IsCodePending(const std::string& email);

// Seconds remaining until a new code can be sent (cooldown).
int GetCodeCooldown(const std::string& email);

} // namespace sf
