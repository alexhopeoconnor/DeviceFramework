using Microsoft.AspNetCore.Mvc;
using EndpointFetcher.Services;

namespace EndpointFetcher.Controllers;

[ApiController]
[Route("api")]
public class EndpointCacheController : ControllerBase
{
    private readonly IEndpointCache _cache;
    private readonly ILogger<EndpointCacheController> _logger;

    public EndpointCacheController(
        IEndpointCache cache,
        ILogger<EndpointCacheController> logger)
    {
        _cache = cache;
        _logger = logger;
    }

    /// <summary>
    /// Schedules a background fetch for an endpoint from a device
    /// POST /api/fetch
    /// Body: { "deviceIp": "192.168.1.56", "endpoint": "/api/status" }
    /// Returns: { "jobId": "abc123def456" }
    /// </summary>
    [HttpPost("fetch")]
    public async Task<IActionResult> FetchEndpoint([FromBody] FetchRequest request)
    {
        try
        {
            _logger.LogInformation("Scheduling fetch for {Endpoint} from {DeviceIp}", request.Endpoint, request.DeviceIp);
            var jobId = await _cache.ScheduleFetchAsync(request.DeviceIp, request.Endpoint);
            return Ok(new { jobId });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to schedule fetch");
            return StatusCode(500, new { error = ex.Message });
        }
    }

    /// <summary>
    /// Gets the status of a fetch job
    /// GET /api/status/{jobId}
    /// </summary>
    [HttpGet("status/{jobId}")]
    public async Task<IActionResult> GetFetchStatus(string jobId)
    {
        var status = await _cache.GetFetchStatusAsync(jobId);

        if (status == null)
        {
            return NotFound(new { error = "Job not found" });
        }

        return Ok(status);
    }

    /// <summary>
    /// Streams a chunk of cached content
    /// GET /api/stream/{endpointHash}?offset=0&chunkSize=512
    /// </summary>
    [HttpGet("stream/{endpointHash}")]
    public async Task<IActionResult> StreamContent(
        string endpointHash,
        [FromQuery] int offset = 0,
        [FromQuery] int chunkSize = 512)
    {
        var chunk = await _cache.GetStreamChunkAsync(endpointHash, offset, chunkSize);

        if (chunk == null)
        {
            return NotFound(new { error = "Content not found" });
        }

        return Ok(chunk);
    }

    /// <summary>
    /// Computes hash for an endpoint (utility endpoint)
    /// POST /api/hash
    /// Body: { "endpoint": "/api/status" }
    /// </summary>
    [HttpPost("hash")]
    public IActionResult GetEndpointHash([FromBody] HashRequest request)
    {
        if (string.IsNullOrEmpty(request.Endpoint))
        {
            return BadRequest(new { error = "Endpoint required" });
        }

        var hash = _cache.ComputeHash(request.Endpoint);
        return Ok(new { endpoint = request.Endpoint, hash });
    }

    /// <summary>
    /// Clears the cache
    /// POST /api/clear
    /// </summary>
    [HttpPost("clear")]
    public IActionResult ClearCache()
    {
        _cache.ClearCache();
        return Ok(new { success = true });
    }
}

public record FetchRequest
{
    public string DeviceIp { get; init; } = string.Empty;
    public string Endpoint { get; init; } = string.Empty;
}

public record HashRequest
{
    public string Endpoint { get; init; } = string.Empty;
}
