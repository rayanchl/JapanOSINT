import SwiftUI

struct SchedulerView: View {
    @EnvironmentObject var apiClient: APIClient
    @Environment(\.theme) private var theme

    @State private var jobs: [SchedulerJob] = []
    @State private var loading = false
    @State private var errorMessage: String?

    var body: some View {
        Group {
            if loading, jobs.isEmpty {
                ProgressView().frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if errorMessage != nil, jobs.isEmpty {
                OfflineStateView(retry: { Task { await load() } })
            } else if jobs.isEmpty {
                OfflineStateView(
                    kind: .empty,
                    title: "No jobs reported.",
                    systemImage: "clock.badge.xmark"
                )
            } else {
                ScrollView {
                    VStack(alignment: .leading, spacing: 12) {
                        Text("Cron jobs").font(.headline).foregroundStyle(theme.text)
                        ForEach(jobs) { job in
                            jobCard(job)
                        }

                        if let err = errorMessage, !jobs.isEmpty {
                            // Inline danger text only for partial errors —
                            // the full-screen OfflineStateView handles the
                            // "no data + offline" case above.
                            Text(err).font(.caption).foregroundStyle(theme.danger)
                        }
                    }
                    .padding()
                }
                .overlay(alignment: .topTrailing) {
                    if loading { ProgressView().padding() }
                }
            }
        }
        .task { if jobs.isEmpty { await load() } }
        .refreshable { await load() }
    }

    private func jobCard(_ job: SchedulerJob) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text(job.id).font(.subheadline.bold()).foregroundStyle(theme.text)
                Spacer()
                if let cron = job.cron {
                    Text(cron)
                        .font(.system(.caption2, design: .monospaced))
                        .padding(.horizontal, 6).padding(.vertical, 2)
                        .background(theme.accent.opacity(0.18), in: Capsule())
                        .foregroundStyle(theme.accent)
                }
            }
            if let d = job.description {
                Text(d).font(.caption).foregroundStyle(theme.textMuted)
            }
            HStack(spacing: 12) {
                if let last = job.last_run {
                    Label(relativeTime(last), systemImage: "clock.arrow.circlepath")
                        .font(.caption2)
                        .foregroundStyle(theme.textMuted)
                }
                if let next = job.next_run {
                    Label(relativeTime(next), systemImage: "alarm")
                        .font(.caption2)
                        .foregroundStyle(theme.success)
                }
            }
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(theme.surfaceElevated, in: RoundedRectangle(cornerRadius: 10))
    }

    private func load() async {
        let api = apiClient.api
        loading = true
        defer { loading = false }
        do {
            let env = try await api.scheduler()
            jobs = env.jobs
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}
