if RUBY_ENGINE == 'ruby'
    require 'simplecov'
    SimpleCov.start
  end
  
  require 'json'
  require 'octokit'
  require 'rspec'
  require 'webmock/rspec'
  require 'base64'
  require 'jwt'
  require 'pry-byebug'
  
  WebMock.disable_net_connect!()
  
  RSpec.configure do |config|
    config.raise_errors_for_deprecations!
    config.before(:all) do
      @test_repo = "#{test_github_login}/#{test_github_repository}"
      @test_repo_id = test_github_repository_id
      @test_org_repo = "#{test_github_org}/#{test_github_repository}"
    end
  end
  
  require 'vcr'
  VCR.configure do |c|
    c.configure_rspec_metadata!
    c.filter_sensitive_data("<GITHUB_LOGIN>") do
      test_github_login
    end
    c.filter_sensitive_data("<GITHUB_COLLABORATOR_LOGIN>") do
      test_github_collaborator_login
    end
    c.filter_sensitive_data("<GITHUB_TEAM_SLUG>") do
      test_github_team_slug
    end
    c.filter_sensitive_data("<GITHUB_TEAM_ID>") do
      test_github_team_id
    end
    c.filter_sensitive_data("<GITHUB_PASSWORD>") do
      test_github_password
    end
    c.filter_sensitive_data("<<ACCESS_TOKEN>>") do
      test_github_token
    end
    c.filter_sensitive_data("<GITHUB_COLLABORATOR_TOKEN>") do
      test_github_collaborator_token
    end
    c.filter_sensitive_data("<GITHUB_CLIENT_ID>") do
      test_github_client_id
    end
    c.filter_sensitive_data("<GITHUB_CLIENT_SECRET>") do
      test_github_client_secret
    end
    c.filter_sensitive_data("<<ENTERPRISE_GITHUB_LOGIN>>") do
      test_github_enterprise_login
    end
    c.filter_sensitive_data("<<ENTERPRISE_ACCESS_TOKEN>>") do
      test_github_enterprise_token
    end
    c.filter_sensitive_data("<<ENTERPRISE_MANAGEMENT_CONSOLE_PASSWORD>>") do
      test_github_enterprise_management_console_password
    end
    c.filter_sensitive_data("<<ENTERPRISE_MANAGEMENT_CONSOLE_ENDPOINT>>") do
      test_github_enterprise_management_console_endpoint
    end
    c.filter_sensitive_data("<<ENTERPRISE_HOSTNAME>>") do
      test_github_enterprise_endpoint
    end
    c.define_cassette_placeholder("<GITHUB_TEST_REPOSITORY>") do
      test_github_repository
    end
    c.define_cassette_placeholder("<GITHUB_TEST_REPOSITORY_ID>") do
      test_github_repository_id
    end
    c.define_cassette_placeholder("<GITHUB_TEST_ORGANIZATION>") do
      test_github_org
    end
    c.define_cassette_placeholder("<GITHUB_TEST_ORG_TEAM_ID>") do
      "10050505050000"
    end
    c.define_cassette_placeholder("<GITHUB_TEST_INTEGRATION>") do
      test_github_integration
    end
    c.define_cassette_placeholder("<GITHUB_TEST_INTEGRATION_INSTALLATION>") do
      test_github_integration_installation
    end
    # This MUST belong to the app used for test_github_client_id and
    # test_github_client_secret
    c.define_cassette_placeholder("<GITHUB_TEST_OAUTH_TOKEN>") do
      test_github_oauth_token
    end
  
    c.before_http_request(:real?) do |request|
      next if request.headers['X-Vcr-Test-Repo-Setup']
      next unless request.uri.include? test_github_repository
  
      options = {
        :headers => {'X-Vcr-Test-Repo-Setup' => 'true'},
        :auto_init => true
      }
  
      test_repo = "#{test_github_login}/#{test_github_repository}"
      if !oauth_client.repository?(test_repo, options)
        Octokit.octokit_warn "NOTICE: Creating #{test_repo} test repository."
        oauth_client.create_repository(test_github_repository, options)
      end
  
      test_org_repo = "#{test_github_org}/#{test_github_repository}"
      if !oauth_client.repository?(test_org_repo, options)
        Octokit.octokit_warn "NOTICE: Creating #{test_org_repo} test repository."
        options[:organization] = test_github_org
        oauth_client.create_repository(test_github_repository, options)
      end
    end
  
    c.ignore_request do |request|
      !!request.headers['X-Vcr-Test-Repo-Setup']
    end
  
    record_mode =
      case
      when ENV['GITHUB_CI']
        :none
      when ENV['OCTOKIT_TEST_VCR_RECORD']
        :all
      else
        :once
      end
  
    c.default_cassette_options = {
      :serialize_with             => :json,
      # TODO: Track down UTF-8 issue and remove
      :preserve_exact_body_bytes  => true,
      :decode_compressed_response => true,
      :record                     => record_mode
    }