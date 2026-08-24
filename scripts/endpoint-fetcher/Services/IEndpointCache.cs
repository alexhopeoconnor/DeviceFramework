namespace EndpointFetcher.Services;

public interface IEndpointCache
{
    Task<string> ScheduleFetchAsync(string deviceIp, string endpoint);
    Task<FetchStatus?> GetFetchStatusAsync(string jobId);
    Task<StreamChunk?> GetStreamChunkAsync(string endpointHash, int offset, int chunkSize);
    string ComputeHash(string endpoint);
    void ClearCache();
}

public record FetchStatus
{
    public string JobId { get; init; } = string.Empty;
    public string EndpointHash { get; init; } = string.Empty;
    public string Status { get; init; } = string.Empty; // "pending", "completed", "failed"
    public int StatusCode { get; init; }
    public long ContentLength { get; init; }
    public string? ErrorMessage { get; init; }
}

public record FetchResult
{
    public string EndpointHash { get; init; } = string.Empty;
    public int StatusCode { get; init; }
    public long ContentLength { get; init; }
    public string? ContentType { get; init; }
    public bool IsValid { get; init; }
    public long FetchTimeMs { get; init; }
}

public record StreamChunk
{
    public string Chunk { get; init; } = string.Empty;
    public int Offset { get; init; }
    public int ChunkSize { get; init; }
    public long TotalSize { get; init; }
    public bool IsLastChunk { get; init; }
}
