import warnings

import github

from . import Framework


class Authentication(Framework.BasicTestCase):
    def testNoAuthentication(self):
        g = github.Github()
        self.assertEqual(g.get_user("jacquev6").name, "Vincent Jacques")

    def testBasicAuthentication(self):
        g = github.Github(self.login, self.password)
        self.assertEqual(g.get_user("jacquev6").name, "Vincent Jacques")

    def testOAuthAuthentication(self):
        g = github.Github(self.oauth_token)
        self.assertEqual(g.get_user("jacquev6").name, "Vincent Jacques")

    def testJWTAuthentication(self):
        g = github.Github(jwt=self.jwt)
        self.assertEqual(g.get_user("jacquev6").name, "Vincent Jacques")

    # Warning: I don't have a secret key, so the requests for this test are forged
    def testSecretKeyAuthentication(self):
        # Ignore the warning since client_{id,secret} are deprecated
        warnings.filterwarnings("ignore", category=FutureWarning)
        g = github.Github(client_id=self.client_id, client_secret=self.client_secret)
        self.assertListKeyEqual(
            g.get_organization("BeaverSoftware").get_repos("public"),
            lambda r: r.name,
            ["FatherBeaver", "PyGithub"],
        )
        warnings.resetwarnings()

    def testUserAgent(self):
        g = github.Github(user_agent="PyGithubTester")
        self.assertEqual(g.get_user("jacquev6").name, "Vincent Jacques")

    def testAuthorizationHeaderWithLogin(self):
        # See special case in Framework.fixAuthorizationHeader
        g = github.Github("fake_login", "fake_password")
        with self.assertRaises(github.GithubException):
            g.get_user().name

    def testAuthorizationHeaderWithToken(self):
        # See special case in Framework.fixAuthorizationHeader
        g = github.Github("ZmFrZV9sb2dpbjpmYWtlX3Bhc3N3b3Jk")
        with self.assertRaises(github.GithubException):
            g.get_user().name
