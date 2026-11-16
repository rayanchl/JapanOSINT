import Foundation

extension AuthSession {
    /// Platform-operator gate. The SERVER decides who is an operator
    /// (`PLATFORM_OPERATOR_EMAILS`, `opgate_check`); the app asks it, exactly
    /// as the web client does, by calling an operator-gated route once per
    /// session (`AuthSession.probeOperatorIfNeeded`) and caching the answer.
    /// Nothing about who the operator is is baked into the binary.
    ///
    /// Operator-only surfaces (raw DB explorer, scheduler, follow log,
    /// platform API keys, repair worker, breach corpus) affect every workspace
    /// on the host, so they are hidden for everyone else. Hiding them is
    /// presentation only: the server still answers 403 to a non-operator.
    var isPlatformAdmin: Bool { isOperator }
}
