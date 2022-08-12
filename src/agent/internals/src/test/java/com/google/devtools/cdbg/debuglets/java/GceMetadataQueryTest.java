package com.google.devtools.cdbg.debuglets.java;

import static com.google.common.truth.Truth.assertThat;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.mockito.Mockito.when;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLStreamHandler;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

@RunWith(JUnit4.class)
public class GceMetadataQueryTest {

  @Rule public final MockitoRule mocks = MockitoJUnit.rule();

  @Mock HttpURLConnection projectIdUrlConnection;
  @Mock HttpURLConnection projectNumberUrlConnection;
  @Mock HttpURLConnection accessTokenUrlConnection;

  private GceMetadataQuery metadataQuery;

  @Before
  public void setUp() throws MalformedURLException {
    URL baseUrl =
        new URL(
            "mock",
            "metadata.service",
            80,
            "/",
            new URLStreamHandler() {
              @Override
              protected HttpURLConnection openConnection(URL url) throws IOException {
                String base = "mock://metadata.service:80/";

                if (url.toString().equals(base + "project/project-id")) {
                  return projectIdUrlConnection;
                }

                if (url.toString().equals(base + "project/numeric-project-id")) {
                  return projectNumberUrlConnection;
                }

                if (url.toString().equals(base + "instance/service-accounts/default/token")) {
                  return accessTokenUrlConnection;
                }

                return null;
              }
            });

    metadataQuery = new GceMetadataQuery(baseUrl);
  }

  @Test
  public void environmentQuerySuccess() throws IOException {
    when(projectIdUrlConnection.getInputStream()).thenReturn(streamFromString("best_project_ever"));
    when(projectNumberUrlConnection.getInputStream()).thenReturn(streamFromString("3456273849"));

    assertThat(metadataQuery.getProjectId()).isEqualTo("best_project_ever");
    assertThat(metadataQuery.getProjectNumber()).isEqualTo("3456273849");
  }

  @Test
  public void environmentQueryCache() throws IOException {
    when(projectIdUrlConnection.getInputStream()).thenReturn(streamFromString("best_project_ever"));
    when(projectNumberUrlConnection.getInputStream()).thenReturn(streamFromString("3459973849"));

    assertThat(metadataQuery.getProjectId()).isEqualTo("best_project_ever");
    assertThat(metadataQuery.getProjectNumber()).isEqualTo("3459973849");
  }

  @Test
  public void environmentQueryFailureProjectId() throws IOException {
    when(projectIdUrlConnection.getInputStream())
        .thenThrow(new IOException())
        .thenReturn(streamFromString("best_project_ever"));

    assertThat(metadataQuery.getProjectId()).isEmpty();
    assertThat(metadataQuery.getProjectId()).isEqualTo("best_project_ever");
  }

  @Test
  public void environmentQueryFailureProjectNumber() throws IOException {
    when(projectNumberUrlConnection.getInputStream())
        .thenThrow(new IOException())
        .thenReturn(streamFromString("3456273849"));

    assertThat(metadataQuery.getProjectNumber()).isEmpty();
    assertThat(metadataQuery.getProjectNumber()).isEqualTo("3456273849");
  }

  @Test
  public void accessTokenSuccess() throws IOException {
    when(accessTokenUrlConnection.getInputStream())
        .thenReturn(
            streamFromString(
                "{\"access_token\":\"ya29mytoken\","
                    + "\"expires_in\":"
                    + (GceMetadataQuery.CACHE_BUFFER_SEC + 2)
                    + ",\"token_type\":\"Bearer\"}"));

    assertThat(metadataQuery.getAccessToken())
        .isEqualTo("ya29mytoken");
  }

  @Test
  public void accessTokenMissingExpirationTime() throws IOException {
    when(accessTokenUrlConnection.getInputStream())
        .thenReturn(
            streamFromString("{\"access_token\":\"ya29mytoken\",\"token_type\":\"Bearer\"}"),
            streamFromString("{\"access_token\":\"ya29mytoken\",\"token_type\":\"Bearer\"}"),
            streamFromString("{\"access_token\":\"ya29mytoken\",\"token_type\":\"Bearer\"}"));

    assertThat(metadataQuery.getAccessToken()).isEqualTo("ya29mytoken");
    assertThat(metadataQuery.getAccessToken()).isEqualTo("ya29mytoken");
    assertThat(metadataQuery.getAccessToken()).isEqualTo("ya29mytoken");
  }

  @Test
  public void badAccessToken() throws IOException, InterruptedException {
    when(accessTokenUrlConnection.getInputStream())
        .thenReturn(
            streamFromString(
                "{\"access_token\":\"firsttoken\","
                    + "\"expires_in\":"
                    + (GceMetadataQuery.CACHE_BUFFER_SEC + 1)
                    + ",\"token_type\":\"Bearer\"}"),
            streamFromString("{\"access_token\":\"secondtoken\",\"token_type\":\"Bearer\"}"));

    assertThat(metadataQuery.getAccessToken()).isEqualTo("firsttoken");
    assertThat(metadataQuery.getAccessToken()).isEqualTo("firsttoken");
    Thread.sleep(1000);
    assertThat(metadataQuery.getAccessToken()).isEqualTo("secondtoken");
  }

  @Test
  public void accessTokenExpiration() throws IOException {
    when(accessTokenUrlConnection.getInputStream())
        .thenReturn(
            // Bad JSON.
            streamFromString("{dfgkjhsdfg , sdfg dsfgdsfg : sdfg23 2323}"),
            // Empty access token attribute.
            streamFromString(
                "{\"access_token\":\"\",\"expires_in\":819,\"token_type\":\"Bearer\"}"),
            // Missing access token attribute.
            streamFromString("{\"expires_in\":819,\"token_type\":\"Bearer\"}"));

    assertThat(metadataQuery.getAccessToken()).isEmpty();
    assertThat(metadataQuery.getAccessToken()).isEmpty();
    assertThat(metadataQuery.getAccessToken()).isEmpty();
  }

  private ByteArrayInputStream streamFromString(String s) {
    return new ByteArrayInputStream(s.getBytes(UTF_8));
  }
}
