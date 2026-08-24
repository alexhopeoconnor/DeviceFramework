using System.Collections.Concurrent;
using System.Security.Cryptography;

namespace EndpointFetcher.Services;

public class EndpointCache : IEndpointCache
{
    private readonly HttpClient _httpClient;
    private readonly ILogger<EndpointCache> _logger;
    private readonly ConcurrentDictionary<string, string> _contentCache = new();
    private readonly ConcurrentDictionary<string, FetchStatus> _fetchJobs = new();

    public EndpointCache(ILogger<EndpointCache> logger, IHttpClientFactory httpClientFactory)
    {
        _logger = logger;
        _httpClient = httpClientFactory.CreateClient();
        _httpClient.Timeout = TimeSpan.FromMinutes(2); // Allow time for slow streaming responses
    }

    public Task<string> ScheduleFetchAsync(string deviceIp, string endpoint)
    {
        var endpointHash = ComputeHash(endpoint);
        var jobId = Guid.NewGuid().ToString("N")[..12]; // Short ID

        // Mark job as pending
        _fetchJobs[jobId] = new FetchStatus
        {
            JobId = jobId,
            EndpointHash = endpointHash,
            Status = "pending"
        };

        // Start background fetch (don't await)
        _ = Task.Run(async () => await PerformFetchAsync(jobId, deviceIp, endpoint, endpointHash));

        _logger.LogInformation("Scheduled fetch for {Endpoint}, job: {JobId}", endpoint, jobId);

        return Task.FromResult(jobId);
    }

    public Task<FetchStatus?> GetFetchStatusAsync(string jobId)
    {
        if (_fetchJobs.TryGetValue(jobId, out var status))
        {
            return Task.FromResult<FetchStatus?>(status);
        }
        return Task.FromResult<FetchStatus?>(null);
    }

    private async Task PerformFetchAsync(string jobId, string deviceIp, string endpoint, string endpointHash)
    {
        var url = $"http://{deviceIp}{endpoint}";

        try
        {
            _logger.LogInformation("Starting background fetch for {Endpoint} (job: {JobId})", endpoint, jobId);

            var response = await _httpClient.GetAsync(url, HttpCompletionOption.ResponseHeadersRead);
            var contentType = response.Content.Headers.ContentType?.MediaType;

            // Read content in chunks
            using var contentStream = await response.Content.ReadAsStreamAsync();
            using var reader = new StreamReader(contentStream);
            var content = await reader.ReadToEndAsync();

            // Cache the content
            _contentCache[endpointHash] = content;

            // Update job status
            _fetchJobs[jobId] = new FetchStatus
            {
                JobId = jobId,
                EndpointHash = endpointHash,
                Status = "completed",
                StatusCode = (int)response.StatusCode,
                ContentLength = content.Length
            };

            _logger.LogInformation(
                "Completed fetch for {Endpoint} (job: {JobId}): {StatusCode}, {Size} bytes → hash: {Hash}",
                endpoint, jobId, (int)response.StatusCode, content.Length, endpointHash);
        }
        catch (HttpRequestException ex)
        {
            _logger.LogError(ex, "HTTP error fetching {Endpoint} (job: {JobId})", endpoint, jobId);

            _fetchJobs[jobId] = new FetchStatus
            {
                JobId = jobId,
                EndpointHash = endpointHash,
                Status = "failed",
                ErrorMessage = ex.Message
            };
        }
        catch (TaskCanceledException ex)
        {
            _logger.LogError(ex, "Timeout fetching {Endpoint} (job: {JobId})", endpoint, jobId);

            _fetchJobs[jobId] = new FetchStatus
            {
                JobId = jobId,
                EndpointHash = endpointHash,
                Status = "failed",
                ErrorMessage = "Timeout"
            };
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to fetch {Endpoint} (job: {JobId})", endpoint, jobId);

            _fetchJobs[jobId] = new FetchStatus
            {
                JobId = jobId,
                EndpointHash = endpointHash,
                Status = "failed",
                ErrorMessage = ex.Message
            };
        }
    }

    public Task<StreamChunk?> GetStreamChunkAsync(string endpointHash, int offset, int chunkSize)
    {
        if (!_contentCache.TryGetValue(endpointHash, out var content))
        {
            return Task.FromResult<StreamChunk?>(null);
        }

        // Clamp chunk size
        if (chunkSize <= 0 || chunkSize > 4096)
            chunkSize = 512;
        if (offset < 0 || offset >= content.Length)
            offset = 0;

        var remainingSize = content.Length - offset;
        var actualChunkSize = Math.Min(chunkSize, remainingSize);
        var chunk = content.Substring(offset, actualChunkSize);
        var isLastChunk = offset + actualChunkSize >= content.Length;

        return Task.FromResult<StreamChunk?>(
            new StreamChunk
            {
                Chunk = chunk,
                Offset = offset,
                ChunkSize = actualChunkSize,
                TotalSize = content.Length,
                IsLastChunk = isLastChunk
            });
    }

    public void ClearCache()
    {
        _contentCache.Clear();
        _fetchJobs.Clear();
        _logger.LogInformation("Cache cleared");
    }

    public string ComputeHash(string endpoint)
    {
        using var sha256 = SHA256.Create();
        var hash = sha256.ComputeHash(System.Text.Encoding.UTF8.GetBytes(endpoint));
        return Convert.ToHexString(hash)[..16]; // First 16 chars
    }
}
