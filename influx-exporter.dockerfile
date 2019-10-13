FROM mcr.microsoft.com/dotnet/core/sdk:3.0 AS build
MAINTAINER Danielv123
RUN mkdir app
WORKDIR /app

# Move project
COPY ["iot_BME280 influx exporter/*.csproj", "./influx_exporter/"]
WORKDIR /app/influx_exporter
RUN dotnet restore

# Move app and stuff
WORKDIR /app/
COPY ["iot_BME280 influx exporter/.", "./influx_exporter"]
WORKDIR /app/influx_exporter
RUN dotnet publish -c Release -o out

# Run application
FROM build AS runtime
WORKDIR /app
COPY --from=build /app/influx_exporter/out ./
ENTRYPOINT ["dotnet", "iot_BME280 influx exporter.dll"]
