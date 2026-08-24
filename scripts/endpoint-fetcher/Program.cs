using EndpointFetcher.Services;
using EndpointFetcher.Controllers;

var builder = WebApplication.CreateBuilder(args);

// Add services
builder.Services.AddSingleton<IEndpointCache, EndpointCache>();
builder.Services.AddControllers();
builder.Services.AddHttpClient();
builder.Services.AddLogging(logging =>
{
    logging.ClearProviders();
    logging.AddConsole();
    logging.SetMinimumLevel(LogLevel.Information);
});

var app = builder.Build();

// Configure middleware
app.UseRouting();
app.MapControllers();

// Health check endpoint
app.MapGet("/health", () => new { status = "ready", timestamp = DateTime.UtcNow });

// CORS
app.UseCors(policy => policy
    .AllowAnyOrigin()
    .AllowAnyMethod()
    .AllowAnyHeader());

app.Logger.LogInformation("Endpoint Fetcher started (listening on ASPNETCORE_URLS)");
app.Run();
